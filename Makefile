all:
	$(MAKE) zlib-compile
	$(MAKE) -C common 
	$(MAKE) sender-compile
	$(MAKE) receiver-compile

clean:
	$(MAKE) zlib-clean
	$(MAKE) sender-clean
	$(MAKE) receiver-clean
	$(MAKE) -C common clean
	$(MAKE) -C t clean

test:
	$(MAKE) -C common
	$(MAKE) -C t
	gcc common/protocol.o t/tests.o -o tests `pkg-config --cflags --libs check,zlib`
	./tests

zlib-compile:
	cd zlib; ./configure; make; cd -

zlib-clean:
	cd zlib; make clean; cd -

sender-compile:
	gcc -c sender.c
	gcc sender.o common/protocol.o zlib/crc32.o -o sender

sender-clean:
	 -rm sender.o sender

receiver-compile:
	gcc -c receiver.c
	gcc receiver.o common/protocol.o zlib/crc32.o -o receiver

receiver-clean:
	-rm receiver.o receiver
