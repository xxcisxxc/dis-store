import torch
import torch.nn as nn
import torch.optim as optim
from torch.optim import lr_scheduler
from torchvision import datasets, models, transforms

from ckpt_manage import CKPTManage, SGD
from check_freq import CFCheckpoint, CFManager, CFMode

def main():
    model = models.resnet18()

    data_transforms = {
        'train': transforms.Compose([
            transforms.RandomResizedCrop(16),
            transforms.RandomHorizontalFlip(),
            transforms.ToTensor(),
            transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
        ]),
        'eval': transforms.Compose([
            transforms.Resize(32),
            transforms.CenterCrop(16),
            transforms.ToTensor(),
            transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
        ]),
    }

    train_dicts = {'train': True, 'eval': False}
    image_datasets = {
        x: datasets.CIFAR10('./data/', train=train_dicts[x], transform=data_transforms[x], download=True) \
            for x in ['train', 'eval']
        }
    dataloaders = {
        x: torch.utils.data.DataLoader(image_datasets[x], batch_size=1024, shuffle=True, num_workers=2) \
            for x in ['train', 'eval']
        }
    dataset_sizes = {x: len(image_datasets[x]) for x in ['train', 'eval']}

    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

    num_ftrs = model.fc.in_features
    model.fc = nn.Linear(num_ftrs, 10)

    ckpt = CKPTManage('ckpt.nvm', model.named_parameters(), model.state_dict())

    model = model.to(device)

    criterion = nn.CrossEntropyLoss()

    #optimizer = optim.SGD(model.parameters(), lr=0.001, momentum=0.9)
    optimizer = SGD(model.parameters(), ckpt, lr=0.001, momentum=0.9)

    #chk = CFCheckpoint(model=model, optimizer=optimizer)
    #cf_manager = CFManager('ckpt.ssd', chk, mode=CFMode.AUTO)

    scheduler = lr_scheduler.StepLR(optimizer, step_size=7, gamma=0.1)

    train_model(model, criterion, optimizer, scheduler, dataloaders, dataset_sizes, 
        device, ckpt)
    #train_model(model, criterion, optimizer, scheduler, dataloaders, dataset_sizes, 
    #    device, cf_manager)

def train_model(model, criterion, optimizer, scheduler, dataloaders, dataset_sizes, 
        device, ckpt, num_epochs=10):
    
    best_acc = 0.0

    for epoch in range(num_epochs):
        print('Epoch {}/{}'.format(epoch, num_epochs - 1))
        print('-' * 10)

        for phase in ['train', 'eval']:
            if phase == 'train':
                model.train()
            else:
                model.eval()

            running_loss = 0.0
            running_corrects = 0

            iteration = -1

            for inputs, labels in dataloaders[phase]:
                iteration += 1
                inputs = inputs.to(device)
                labels = labels.to(device)

                optimizer.zero_grad()

                with torch.set_grad_enabled(phase == 'train'):
                    outputs = model(inputs)
                    _, preds = torch.max(outputs, 1)
                    loss = criterion(outputs, labels)

                    if phase == 'train':
                        loss.backward()
                        print("Epoch:%d Iteration:%d" % (epoch, iteration), end="\t")
                        optimizer.step()
                        #ckpt.weight_update()

                running_loss += loss.item() * inputs.size(0)
                running_corrects += torch.sum(preds == labels.data)
            
            if phase == 'train':
                scheduler.step()

            epoch_loss = running_loss / dataset_sizes[phase]
            epoch_acc = running_corrects.double() / dataset_sizes[phase]

            print('{} Loss: {:.4f} Acc: {:.4f}'.format(
                phase, epoch_loss, epoch_acc))

            if phase == 'eval' and epoch_acc > best_acc:
                best_acc = epoch_acc

        print()

    print('Best eval Acc: {:4f}'.format(best_acc))

    ckpt.wait_end()

if __name__ == '__main__':
    main()