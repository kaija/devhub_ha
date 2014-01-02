
CFLAGS=
LDFLAGS=-lcurl libbcm2835.a
all: report
report: devhub.o
	$(CC) -o devhub devhub.o $(CFLAGS) $(LDFLAGS)
clean:
	rm *.o devhub -rf
install:
	cp script/devhub /etc/init.d/ -rf
	cp devhub /bin/ -rf
	echo "/bin/devhub &" >> /etc/rc.local
