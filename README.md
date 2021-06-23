# ctwz 
ctwz is a lossless compressor based on the Context Tree Weighting method<sup>[1](#1)</sup>, using byte-level contexts and bit-level prediction. It is mainly good for large files.

## Build 
To build use cmake 
```
$ cmake .
$ make
```
or simply compile with C++17. 
## Usage
```
$ ./ctwz -h
ctwz:
	Context tree weighting compressor
	author: Meijke Balay <mysatellite99@gmail.com>
usage:
	encode: ctwz [-d depth] file
	decode: ctwz -x file
```
`-d` Specifies the depth of the context trees (default=8). Greater depths can improve compression for large files but require more memory and computation.  

## Benchmarks
Some results on the [Canterbury Corpus](https://corpus.canterbury.ac.nz/descriptions/#large) using depth 12 ctwz, with gzip (Lempel-Ziv) for comparison

|file | size(bytes) | ctwz | gzip
--- | --- | --- | ---
| E.coli | 4638690| 1209950| 1342009
|bible.txt | 4047392 |735435| 1177372
|world192.txt | 2473400| 561751| 724995 
|kennedy.xls | 1029744| 167460 | 204016
|ptt5 	| 513216 | 55562 | 56482
|plrabn.txt| 481861 |151034 | 194357
|alice29.txt|152089| 47817 | 54428
|asyoulik.txt | 125179 | 42720 | 54428

## Todo
- [ ] Parallelize context tree computations
- [ ] Lower memory requirements with pruning

## References
<div><a name="1">1</a>: Willems, Shtarkov, Tjalkens. "The Context-Tree Weighting Method: Basic Properties"</div>
