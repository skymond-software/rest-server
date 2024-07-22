# Skymond REST Server

The Skymond REST server is the core that the Skymond web services are built on.
It is a pure C implementation that utilizes the C.next libraries.  Having a
pure C implementation allows us have virtually zero external dependencies.  This
gives us the ability to run our software in environments that would not
otherwise be possible.  Our software runs in Linux and Windows environments as
far back as 2007.

# Major Components

There are two major components to our REST server implementation:  The web
interface and the database interface.

## Web Interface

The web interface is composed of two sub-components:  A web server and a web
client.  This allows for bi-directional communication between our web services.

### Web Server

WebServerLib holds the code for the web server.  Among other things, the
webServerCreate constructor takes a pointer to a WebService object that
describes the external APIs and callbacks used to service them.  This allows
the API to be extended very easily.

### Web Client

WebClientLib holds the code for the web client.  Calls may be either SOAP or
JSON over HTTP.

## Database Interface

While databse connections have to be established via implementation-specific
constructors (currently either sqliteInit or mariaDbInit), all database
interactions happen via the abstraction provided in DbInterface.h.
DbClientLib.c is the current implementation of this interface.  By design, this
interface provides abstractions for database operations on a per-table basis.
It does *NOT* support joins or other relational database operations.  This gives
us the flexibility to move to a non-relational database in the future.

