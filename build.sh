#!/bin/sh

echo "Compiling client.cpp"
g++ -o requ -Wall src/client/client.cpp
echo "<<client.ccp done>>"

echo "Compiling server.cpp"
g++ -o serv -Wall src/server/server.cpp
echo "<<server.cpp done>>"
