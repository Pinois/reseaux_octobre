all:
	$(MAKE) zlib-compile

clean:
	$(MAKE) zlib-clean

zlib-compile:
	cd zlib; ./configure; make; cd -

zlib-clean:
	cd zlib; make clean; cd -
