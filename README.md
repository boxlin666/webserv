_This project has been created as part of the 42 curriculum by helin and yanzhao._

# Webserv 

## Description
Webserv is an HTTP/1.1 web server built in C++98. It can host static websites and run CGI scripts just like Nginx.

### Project Goal 
---
To understand how network protocols work and to implement a fully functional HTTP server from scratch.

### Key Features
---
* **I/O Multiplexing**: Non-blocking connection management using `poll()`.
* **Nginx-Style Config**: Easy server setup via a simple configuration file.
* **Supported Methods**: Full support for `GET`, `POST`, `HEAD`, and `DELETE`.
* **Virtual Hosting**: Hosts multiple websites on the same port via the `Host` header.
* **Static File Serving**: Fast delivery of HTML, CSS, JS.
* **Autoindexing**: Automatically lists directory contents if `index.html` is missing.
* **File Management**: Supports file uploads (`POST`) and file removal (`DELETE`).
* **CGI Gateway**: Executes Python and PHP scripts for dynamic content.
* **Redirect Handling**: Supports URL forwarding with HTTP `301`/`302`/`303`/`307`/`308` status codes.
* **Session & Cookies**: Handles user login, authentication, and logout via Python CGI.



## Instructions
### Prerequistes
---
You need a C++ compiler (`c++`) and `make`.

### Compilation & Management
You can manage the project compilation using the following `Makefile` commands:

```bash
# Compile the webserv executable (C++98 compliant)
make

# Remove all object files (.o)
make clean

# Remove all object files and the webserv executable
make fclean

# Recompile the entire project from scratch
make re
```

### Runing the Server

```./webserv ./conf/default.conf```

### Quick test

```bash
curl -v http://localhost:8080/ #GET
curl -I http://localhost:8080/ #HEAD
curl -v -X POST -d "this is a test.txt file" http://localhost:8080/uploads/test.txt #POST
curl -v -X POST http://localhost:8080/uploads -F "file=@/home/login_name/images.jpg" #POST jpg
curl -v -X DELETE http://localhost:8080/uploads/test.txt #DELETE
```

### Siege test
---
Here is the step-by-step guide to download, install, and run the `siege` benchmarking tool:

```bash
# 1. Download and Extract
wget http://download.joedog.org/siege/siege-4.0.7.tar.gz
tar -xzf siege-4.0.7.tar.gz
cd siege-4.0.7

# 2. Fix compilation errors (if any occur)
sed -i 's/^int \(.*\)();$/\/\/ int \1();/' src/setup.h

# 3. Configure, compile and install to your local home directory
./configure --prefix=$HOME/.local
make CFLAGS="-g -O2 -std=gnu89 -w"
make install

# 4. Add to PATH and refresh environment variables
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 5. Run benchmarking

# Note: Ensure your ./webserv is running with a valid configuration before execution.

# Case A: Tests 15 users making 10 requests each on an empty page without delays to check basic setup.
siege -c 15 -r 10 -b http://127.0.0.1:8080/empty.html

# Case B: Floods the server with 50 users at max speed for 30 seconds to test basic stability.
siege -c 50 -t 30s -b http://127.0.0.1:8080/

# Case C: Runs 40 users for 1 minute.
siege -c 40 -t 1m -b http://127.0.0.1:8080/

# Case D: Tests 100 users making 20 requests each with random 1 second delays.
siege -c 100 -d 1 -r 20 http://127.0.0.1:8080/

```

### Resource Leak Verification 
---
Monitor the Resident Set Size (RSS) memory footprint using:
```bash
watch -n 1 "ps -o rss,vsz,pid,comm -p \$(pidof ./webserv)"
```

Monitor the fd numbers during the executing of webserv program:
```bash
watch -n 1 "ls -l /proc/\$(pidof ./webserv)/fd | wc -l"
```

### Hanging Connection Verification
---
Inspect TCP socket state lifecycle using:
```bash
netstat -an | grep 8080
```


### Tstest run
---
Here is the guide to setting up and running typescript test for this project.

```bash
# 1. Requirements
# Node.js (v18 or higher recommended)
# npm (usually comes with Node.js)

# 2. Quick Setup
# If you are on the 42 iMacs or your own Linux environment, run this once to install the testing dependencies:
npm install

# 3. How to run tstest
#Start the webserv program with default.conf configuration file:
./webserv ./conf/default.conf

#Execute the tstest program:
npm test
```

### Pytest run
---
Here is the guide to setting up and running pytest for this project.

```bash
# 1. Requirements
# Python 3 (Ensure it is available at `/usr/bin/python3` to match the server's `cgi_path` configuration)
# pytest (Python testing framework)

# 2. Quick Setup
# If you are on the 42 iMacs or your own Linux environment, install `pytest` using `pip`:
pip install -r requirement.txt

# 3. How to run pytest
#Execute the pytest program without starting the webserv program:
pytest
```

# Resources
* [Socket Programming in C (Video)](https://www.youtube.com/watch?v=mStnzIEprH8&t=972s) - A tutorial video demonstrating how to build a basic web server from scratch.
* [Socket Programming in C (Playlist)](https://www.youtube.com/watch?v=_lQ-3S4fJ0U&list=PLPyaR5G9aNDvs6TtdpLcVO43_jvxp4emI) - A guide on building a mini user agent and a server, showcasing how the two programs interact with each other.
* [Writing an Nginx-like Web Server from Scratch in C++](https://www.alimnaqvi.com/blog/webserv) - A blog that covers the essential backbone architecture of building a web server.
* [MDN HTTP Status Reference](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status) - The official standard guide for standard HTTP response behaviors and status codes.
* [RFC 7230: Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230) - The official specification for parsing HTTP requests, header fields, and chunked transfer encoding.
* [RFC 7231: Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231) - The reference guide for HTTP methods (`GET`, `POST`, `DELETE`) and expected response payloads.
* [RFC 3875: The Common Gateway Interface (CGI) v1.1](https://datatracker.ietf.org/doc/html/rfc3875) - The requirements for executing CGI scripts and handling environment variables.

### How do we use AI in our project?
We built this web server by reading RFC 7230/7231 to learn the official HTTP rules, and used AI like a personal tutor to help us clear up confusing parts and plan stress tests. When we had to make trade-offs, we used AI to think through the options and chose what worked best for us.