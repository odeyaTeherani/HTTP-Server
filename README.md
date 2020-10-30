# HTTP-Server

This program is an implementation of an HTTP server. this is no implement the full HTTP specification, but only a very limited subset of it.
The implementation is an HTTP server that:
 * Constructs an HTTP response based on client's request.
 * Sends the response to the client.

## How to use it
Compile the server with the –lpthread flag <br>
run: ```server <port> <pool-size> <max-number-of-request>``` <br>
Port is the port number your server will listen on. <br> pool-size is the number of threads in the pool. <br> number-of-request is the maximum number of request your server will handle before it destroys the pool.

## Program Description
there is two source files, server.c and threadpool.c. <br>
 * <b>The server</b> handle the connections with the clients. when using TCP, a server
creates a socket for each client it talks to. In other words, there is always one socket where the server listens to connections and for each client connection request, the server opens another socket. In order to enable multithreaded program, the server create threads that handle the connections with the clients.  Since, the server should maintain a limited number of threads, it constructs a thread pool. In
other words, the server creates the pool of threads in advanced and each time it needs a thread to handle a client connection, it takes one from the pool or enqueue the request if there is no available thread in the pool. 

 * <b>The threadpool</b> is implemented by a queue. When the server gets a connection (getting back from accept()), it
should put the connection in the queue. When there will be available thread (can be immediately), it will
handle this connection (read request and write response).

## Program flow
1. Server creates pool of threads, threads wait for jobs.
2. Server accept a new connection from a client (aka a new socket fd)
3. Server dispatch a job - call dispatch with the main negotiation function and fd as a parameter. dispatch will add work_t item to the queue.
4. When there will be an available thread, it will takes a job from the queue and run the
negotiation function. 

## submitted files
  * <b>threadpool.c</b><br>
This file contains the implementation of the methods from the header file 'threadpool.h'.
So that different actions can be made on the threadpool, such as initialization, dispatch, dowork, destroy
and various actions on the job queue.

  * <b>server.c</b><br>
This file contains a main that receiving array of arguments.
The first argument is the port number, followed by the number of threads and the number of requests the server can receive.
After checking the input, the server opens a socket and by using the threadpool handles clients' http requests.
Depending on the type of request - any error, request for a folder or file, etc., the server returns a reply to each client through the socket.
  

## additional functions
   * Create Response- The main method: This function is actually sent as an argument to the dispatch function.
check input, write a response and closing socket.
   * Bad Response- Create an error message depending on the problem, and write to the socket.
   * File Response- Create a response to a file request, and write it to a socket.
   * Directory Response- Create a response to directory request, and write its content to the socket.
   * OK Response- Create a header response of a valid request.
   * Load File- load file and write it to socket.
   * check permissions- return 1 if there is no permissions in path. 0-otherwise.
   * badUssage- show an error meesage and exit(EXIT_FAILURE).
