# Lite-Server
<h3>Project Overview:</h3>
Lite-Server is an ongoing project aiming to be an efficient and secure HTTP/1.1 server. Currently, Lite-Server only supports Linux however could be easily ported to BSD operating systems by replacing the dependency on epoll with kqueue, likely along with a few other minor changes.

<h3>Features:</h3>

- Logging with Combined Log Format, connections, disconnections, warnings, errors, debugging information and the ability to toggle any of these including disabling logging completely

- GET requests supported

- POST requests supported - can take in a user defined function which is associated with a provided string (the request-target of the HTTP request)

- Basic defences against directory traversal and slowloris attacks

- Can handle a large amount of connections concurrently due to epoll

<h3>Priority features to implement:</h3>

- Support for other HTTP methods e.g. HEAD, PUT etc
  
- The ability to run multiple workers on a server (have multiple listening sockets for a single server which can answer requests independently of one another)

- Using configuration files instead of C to do everything making it easier to use

- The ability to call a user defined function provided with any request-target (not just for POST requests)


<h3>What and how can you contribute?</h3>

- If you want to contribute then anything to the project then the priority features to implement is a good start or if you have any good ideas yourself then feel free to ask me

<h3>Currently working on:</h3>

- Investigating performance gains from disabling Nagle's algorithm
