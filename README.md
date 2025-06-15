# Woodshop MQTT Project

This project consists of a C++ application that connects to an MQTT broker. The application is containerized using Docker, and the setup is managed with Docker Compose.

## Project Structure

```
woodshop_mqtt
├── src
│   └── main.cpp          # Main implementation of the C++ application
├── Dockerfile             # Dockerfile for building the C++ application container
├── docker-compose.yml     # Docker Compose file for setting up the application and MQTT broker
└── README.md              # Project documentation
```

## Requirements

- Docker
- Docker Compose

## Building and Running the Application

1. Clone the repository:

   ```
   git clone <repository-url>
   cd woodshop_mqtt
   ```

2. Build and run the containers using Docker Compose:

   ```
   docker-compose up --build
   ```

3. The C++ application will connect to the MQTT broker, which is exposed on the default MQTT port (1883).

## Accessing the MQTT Broker

The MQTT broker will be accessible at `mqtt://localhost:1883`. You can use any MQTT client to connect to it.

## Stopping the Application

To stop the running containers, use:

```
docker-compose down
```

## License

This project is licensed under the MIT License. See the LICENSE file for more details.