# http-servers

## Project Overview 

This project implements two HTTP servers in C that handle both text and image responses. The first is a single-threaded HTTP server that supports one client connection at a time. The second is a multithreaded HTTP server built by extending the single-threaded implementation.

The multithreaded HTTP server uses a thread pool design to support multiple concurrent client connections. A main thread is responsible for accepting new clients and closing client connections. Worker threads in the thread pool handle HTTP requests and responses. This approach avoids the overhead of repeatedly creating and destroying threads. Both servers rely on TCP socket programming for client-server communication. 

## Features:

A multithreaded server that creates a thread pool to handle multiple clients

Direct communication between client and server for a single-threaded server

Supports HTTP requests and responses between clients and the server

## Skills Developed: 

TCP socket programming in C

Server-side communication design

HTTP requests and response parsing

Signal handling for server termination

Robust error checking for socket operation, file I/O, and thread management

## Collaboration:

This project was completed in collaboration with another student. Some starter code and helper functions were provided by the course instructors. 
	
