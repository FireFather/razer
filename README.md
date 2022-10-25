# razer
is a fast & highly optimized UCI chess engine 

It's loosely based on Stuckfish https://github.com/MaxCarlson/StuckFish by Max Carlson

Razer has been largely re-designed, greatly improved, and is completely re-factored and optimized according to thorough analysis by Clang, ReSharper C++, and Visual Studio Code Analysis.

See: https://www.jetbrains.com/help/resharper/Reference__Code_Inspections_CPP.html for details


It is significantly stronger than the original (~2650 vs ~2500).

  
- Games Completed = 16384 of 16384 (Avg game length = 9.551 sec)
- Settings = Gauntlet/32MB/1000ms+100ms/M 500cp for 6 moves, D 120 moves/EPD:book.epd(31526)
- Time = 10845 sec elapsed, 0 sec remaining

|                       |                        |                        |                       |                       |                       |
| --------------------- |  --------------------- |  --------------------- | --------------------- | --------------------- | --------------------- |
|1. Razer 0187          |6005.5/16384| 4965-9338-2081| (L: m=2865 t=0 i=0 a=6473)|	(D: r=823 i=549 f=70 s=13 a=626)|	(tpm=90.9 d=12.50 nps=1664972)|
|2. Monolith 1.0        |1590.5/2048|	1449-316-283|	  (L: m=1 t=0 i=0 a=315)|	    (D: r=113 i=66 f=11 s=3 a=90)|    (tpm=95.5 d=10.37 nps=1774998)|
|3. Arminius 2018-12-23 |1320.5/2048|	1182-589-277|	  (L: m=2 t=0 i=0 a=587)|	    (D: r=114 i=69 f=6 s=1 a=87)|	    (tpm=66.9 d=7.81 nps=1839994)|
|4. Octochess r5190     |1420.0/2048|	1303-511-234| 	(L: m=2 t=0 i=0 a=509)|	    (D: r=84 i=47 f=13 s=3 a=87)|	    (tpm=95.5 d=10.33 nps=1215985)|
|5. Glaurung 1.2.1      |1325.5/2048|	1224-621-203| 	(L: m=2 t=0 i=0 a=619)|	    (D: r=75 i=65 f=6 s=0 a=57)|      (tpm=109.6 d=9.13 nps=1300987)|
|6. Fridolin 3.00       |1117.0/2048|	969-783-296 | 	(L: m=34 t=0 i=0 a=749)|    (D: r=111 i=116 f=0 s=2 a=67)|	  (tpm=77.1 d=9.23 nps=2336914)|
|7. Bobcat 3.25         |1285.0/2048|	1155-633-260| 	(L: m=16 t=0 i=0 a=617)|	  (D: r=106 i=55 f=14 s=1 a=84)|	  (tpm=109.6 d=11.38 nps=1688967)|
|8. Fruit 2.1           |1147.5/2048|	1027-780-241|	  (L: m=2 t=0 i=0 a=778)|	    (D: r=106 i=63 f=6 s=1 a=65)|   	(tpm=95.0 d=8.53 nps=0)|
|9. Wyldchess 1.51      |1172.5/2048|	1029-732-287| 	(L: m=4 t=0 i=0 a=728)|	    (D: r=114 i=68 f=14 s=2 a=89)|	  (tpm=99.0 d=11.26 nps=0)|
 
|                        |           |           |           |           |           |
| ---------------------- | --------- | --------- | --------- | --------- | --------- |
| PLAYER | RATING| ERROR| POINTS| PLAYED| WIN|
|1 Monolith 1.0|         2870.9|   11.8|   1590.5|   2048|   77.7%|
|2 Octochess r5190|      2795.4|   11.1|   1420.0|   2048|   69.3%|
|3 Glaurung 1.2.1|       2758.7|   11.4|   1325.5|   2048|   64.7%|
|4 Arminius 2018-12-23|  2756.8|   10.2|   1320.5|   2048|   64.5%|
|5 Bobcat 3.25|          2743.7|   12.0|   1285.0|   2048|   62.7%|
|6 Wyldchess 1.51|       2703.5|   11.3|   1172.5|   2048|   57.3%|
|7 Fruit 2.1|            2694.7|   10.9|   1147.5|   2048|   56.0%|
|8 Fridolin 3.00|        2684.2|   10.6|   1117.0|   2048|   54.5%|
|9 Razer 0187|           2652.2|    3.8|   6005.5|  16384|   36.7%|

