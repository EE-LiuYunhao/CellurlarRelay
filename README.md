## Brief

The repository contains a Raspberry Pi project to use a SIM card with a SIM7600 module. 
It provides the following functionalities: 

1. A service that connects to the SIM7600 module via UART serial port using AT commands.
   The service is by default sleeping, until it receives a phone call, an SMS emssage or
   a command from the frontend app:
   1. On the reception of an SMS message, it relays the message to a pre-configured email address;
   1. On the reception of a phone call, it will refuse it if no frontend app is running. Otherwise,
      it will prompt the frontend and ask the frontend to determine whether to answer it or not.
1. A frontend app can wake up the service and ask it to execute other operations permitted by
   AT commands, including:
   1. Querying the current SIM card and cellular status
   1. Answering phone calls
   1. Configuring the service settings and SIM card preferences


## Set up

### Hardware setup

The project is prototyped and run on using a Raspberry Pi Zero 2 with a WaveShare SIM7600X Hat
plugged onto the Raspberry Pi Zero's GPIO pins. 
<div align="center">
<img width="30%" alt="waveshare SIM7600 board plugged onto Raspberry Pi Zero 2's GPIO pins" src="https://github.com/user-attachments/assets/3a5fa55a-295e-4028-bf6c-5389318fd745" />
</div>

The Raspberry Pi Zero 2 must have its UART debugging disabled, and UART serial port opened. 

### Installation

#### Step 1 Install the executables

1. Install CMake
   
   __*TODO*__

1. Pull the codes and build

   __*TODO*__

1. Copy the output library and executables to `/usr/local/lib` and `/usr/local/bin`

#### Step 2 Modification of the systemd

1. Create a tmpfs mount for the service logs
   
   __*TODO*__
   
1. Create a log file under the mount
   
   __*TODO*__
   
1. Config the logrotate to circulate the log
   
   __*TODO*__
   
1. Add the service config in the systemd to automatically boot the service executable
   
   __*TODO*__


## Code structure

| Subdirectory | Description|
|--------------|-----------------------------------|
| `utils` | The subdirectory contains the files to be shared by both the service and the frontend app, such as the logger | 
| `uart_service` | The source files for the backend service, using UART to communicate with the SIM7600 module | 
| `cmd_app` | A command-line application communicating the service |



  
