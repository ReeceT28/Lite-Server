# Lite-Server
Working on implementing my own web server to learn new stuff using only the POSIX standard library in C.
Inspired by nginx, picohttpparser and other cool web server related projects in certain areas. 

Things I have done so far:
- Memory pooling
- HTTP/1.1 parsing
- Learnt how to implement hash table and trie data structures
- Reading the RFC specifications to help inform my design (but also bending the rules in some places like popular services like nginx do for performance and convenience, e.g. valid characters for a HTTP header name)
- Learning more about how unix/ linux works e.g. file descriptors

Some things I am going to do:
- Event loops using epoll
- Learning sockets in more depth
- Generating HTTP responses
- Preventing things like directory traversal
- More thorough testing for potential vulnurabilities
