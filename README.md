# Data Logging library

This repository contains components to record fixed- and variable- size data pieces in the file system
and to retrieve data recorded within a given time range.

## Recording

```
 ------------------                         --------------------------------------------------------------
 |   Client App   |                         |                        Logger App                          |
 ------------------                         --------------------------------------------------------------
                                            | Listening thread (per client)     |   Writing thread       |               
                                            --------------------------------------------------------------
1. Initialization
- LogClient client("Stream name", record_size);
- client.connect(ip, port)            -->     - creates a new thread

2. Logging
- client.logData(record, time_ns)     -->     - queues the data and notifies
                                                the writing thread.          ->  - deletes the oldest files
                                                                                   if there is not enough space
                                                                                 - creates new file(s), if needed
                                                                                 - adds records to the file(s)
```

## Reading

```
 ------------------                         --------------------------------------------------------------
 |   Client App   |                         |                        Scanner App                         |
 ------------------                         --------------------------------------------------------------

1. Initialization
- LogReadClient client;
- client.connect(ip, port)                   -->    - creates a new thread

2. Get list of streams for given time range
- client.get(start_time, end_time)           -->    - scans the folder for all files matching the given time range
                                             <--    - returns list of files
3. Get data pieces for a given file name
- client.getData(file, start_time, end_time) -->    - extracts all records for given time range
                                             <--    - returns list of records
```

## Deliverables

### Share libraries

#### libLogWriter.so 
Library implements a TCP server to listen for logging requests and to manager the storage folder.

#### libLogReader.so
Library implements a TCP server to handle requests for the streams and data pieces available for a given time range.

#### libLogClient.so
Library implements client's side functionality for logging data as well as to retrieve it.

### Sample apps
There are several apps which are intended to demonstrate the logging use:

#### Logger example.
File logger_demo.cpp implements both logger and scanner functionality.
It uses /tmp folder to store the log files and spins the Logger server at port 59999.
Port 49998 is used to handle reading requests.

#### Logging client example
File client_demo.cpp implements a simple app which creates logging stream "Sample" with
variable length records, then reads lines from the standard
input (keyboard) and sends them to the logger.

#### Reading client example
File reader_demo.cpp implements an example of how to read logs.
First, it requests all streams available by setting start time to 0 and end time - to UINT64_MAX.
It prints the response to the console and then waits for user to enter the file name.
This name will be sent to the scanner app to get back the list of all data records in
that file.
