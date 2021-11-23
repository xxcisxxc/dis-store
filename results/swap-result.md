#### SSD swap

`/dev/sda3`:

1M		Ops: 935 TPS: 93 Net_rate: 0.1M/s

2M		Ops: 7720 TPS: 772 Net_rate: 0.9M/s

4M		Ops: 44736 TPS: 4473 Net_rate: 5.0M/s

8M		Ops: 112361 TPS: 11235 Net_rate: 12.6M/s

16M  	Ops: 201953 TPS: 20194 Net_rate: 22.6M/s

32M 	 Ops: 352188 TPS: 35216 Net_rate: 39.4M/s

64M  	Ops: 748432 TPS: 74838 Net_rate: 81.4M/s

128M	Ops: 1031656 TPS: 103159 Net_rate: 103.9M/s

256M	Ops: 1071668 TPS: 107159 Net_rate: 106.3M/s

512M	Ops: 1034670 TPS: 103460 Net_rate: 104.0M/s

1G		 Ops: 1055503 TPS: 105543 Net_rate: 105.5M/s

2G		 Ops: 1026394 TPS: 102633 Net_rate: 103.5M/s

4G		 Ops: 1042485 TPS: 104242 Net_rate: 104.6M/s

8G		 Ops: 1039366 TPS: 103930 Net_rate: 104.3M/s





Test run time:

64M		Run time: 33.8s Ops: 2000000 TPS: 59218 Net_rate: 46.4M/s

128M	  Run time: 19.1s Ops: 2000000 TPS: 104625 Net_rate: 82.6M/s

128M 	 Run time: 19.1s Ops: 2000000 TPS: 104912 Net_rate: 82.9M/s

512M	  Run time: 19.8s Ops: 2000000 TPS: 100902 Net_rate: 79.5M/s



#### PM swap

`/dev/pmem0`:

2M		Ops: 260862 TPS: 26084 Net_rate: 29.2M/s

4M		Ops: 346383 TPS: 34636 Net_rate: 38.8M/s

8M		Ops: 427779 TPS: 42775 Net_rate: 47.9M/s

16M	  Ops: 512575 TPS: 51254 Net_rate: 57.3M/s

32M	 Ops: 693839 TPS: 69379 Net_rate: 76.4M/s

64M	 Ops: 988468 TPS: 98840 Net_rate: 100.5M/s

128M   Ops: 1047080 TPS: 104701 Net_rate: 104.8M/s

256M   Ops: 1038311 TPS: 103824 Net_rate: 104.3M/s

...



#### ramdisk swap

`/dev/ram0......ram16`:

2M		Ops: 262885 TPS: 26287 Net_rate: 29.4M/s

4M		Ops: 346440 TPS: 34642 Net_rate: 38.8M/s

8M		Ops: 428965 TPS: 42894 Net_rate: 48.0M/s

16M	 Ops: 534188 TPS: 53415 Net_rate: 59.8M/s

32M	 Ops: 699308 TPS: 69926 Net_rate: 76.9M/s

64M	 Ops: 998713 TPS: 99864 Net_rate: 101.2M/s

128M   Ops: 1052084 TPS: 105201 Net_rate: 105.2M/s

256M   Ops: 1005399 TPS: 100533 Net_rate: 101.9M/s

...



#### Infiniswap PM

`/dev/infiniswap0`:

1M			 Ops: 1359 TPS: 136 Net_rate: 0.2M/s

2M			 Ops: 9613 TPS: 961 Net_rate: 1.1M/s

4M 			Ops: 41291 TPS: 4129 Net_rate: 4.6M/s

8M			 Ops: 91519 TPS: 9151 Net_rate: 10.2M/s

16M		  Ops: 183116 TPS: 18310 Net_rate: 20.5M/s

32M 		 Ops: 341591 TPS: 34157 Net_rate: 38.2M/s

64M		  Ops: 740487 TPS: 74044 Net_rate: 80.7M/s

128M		Ops: 1041493 TPS: 104143 Net_rate: 104.5M/s

256M		Ops: 1027333 TPS: 102726 Net_rate: 103.5M/s

512M 	   Ops: 1032571 TPS: 103250 Net_rate: 103.7M/s

1G			 Ops: 1025113 TPS: 102504 Net_rate: 103.4M/s

...



#### Infiniswap DRAM

1M			Ops: 1252 TPS: 125 Net_rate: 0.1M/s

2M			Ops: 7063 TPS: 706 Net_rate: 0.8M/s

4M			Ops: 37411 TPS: 3741 Net_rate: 4.2M/s

8M			Ops: 100459 TPS: 10045 Net_rate: 11.2M/s

16M		  Ops: 180021 TPS: 18001 Net_rate: 20.1M/s

32M 		 Ops: 334941 TPS: 33492 Net_rate: 37.5M/s

64M		  

128M		