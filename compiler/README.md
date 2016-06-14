The compiler is composed of three major components; the parser, the tree and the
backend. The parser lexes and parses the input files and then it builds a tree 
representation of the code. A series of passes are then run on the tree. The 
last pass is the transform and typecheck pass. The typecheck pass verifies that
all types in the program are compatible. The transform pass transforms the code
into a more low-level form that can be translated into C++.

The compiler does not free any memory since it just creates the necessary data 
structures to represent the program and then exits.
