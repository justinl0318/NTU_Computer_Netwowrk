CC = gcc
CXX = g++
LINK = -lrt -lssl -lcrypto -lz
CFLAG = -std=c++20 -g

SENDER = sender.cpp
RECEIVER = receiver.cpp
AGENT = agent.cpp
HEADER = def.h
CRC32 = crc32.cpp
SHA256 = sha256.cpp
SND = sender
RCV = receiver
AGT = agent
CRC = crc
SHA = sha

all: sender receiver agent
  
sender: $(SENDER) $(HEADER)
	$(CXX) $(SENDER) -o $(SND) $(LINK) $(CFLAG)
receiver: $(RECEIVER) $(HEADER)
	$(CXX) $(RECEIVER) -o $(RCV) $(LINK) $(CFLAG)
agent: $(AGENT) $(HEADER)
	$(CXX) $(AGENT) -o $(AGT) $(LINK) $(CFLAG)
crc32: $(CRC32)
	$(CXX) $(CRC32) -o $(CRC) $(LINK) $(CFLAG)
sha256: $(SHA256)
	$(CXX) $(SHA256) -o $(SHA) $(LINK) $(CFLAG)

.PHONY: clean

clean:
	rm $(SND) $(RCV) $(AGT) $(CRC) $(SHA)
