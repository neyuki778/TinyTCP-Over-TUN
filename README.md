Stanford CS 144 Networking Lab
==============================

These labs are open to the public under the (friendly) request that to
preserve their value as a teaching tool, solutions not be posted
publicly by anybody.

Website: https://cs144.stanford.edu

To build a create a needed environment(make sure you have downloaded Docker),
When you first time creating : `docker-compose build`


## Docker Setup

This project supports development using Docker for a consistent environment.

### Prerequisites
- Install Docker on your system. Visit [Docker's official website](https://www.docker.com/get-started) for installation instructions.

### Building the Docker Image
To build the Docker image for the first time:
```
docker-compose build
```

### Running the Container
To start a new container and enter the bash shell (container will be removed after exit):
```
docker-compose run --rm dev /bin/bash
```

Alternatively, to start the container in the background:
```
docker-compose up -d dev
```
Then attach to it:
```
docker-compose exec dev /bin/bash
```

### Inside the Container
Once inside the container, you can set up and build the project as usual:

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`

### Stopping and Cleaning Up
To stop the background container:
```
docker-compose down
```

To remove the built image:
```
docker-compose down --rmi local
```
