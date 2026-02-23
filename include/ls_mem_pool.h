#pragma once
/* TO DO: work on memory pooling */
/* IDEA: each request has a memory pool allocated to it which is big enough for things that should be allocated on the heap, probably things like hash map for header lines */