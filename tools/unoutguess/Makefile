
unoutguess: md5.o arc.o iterator.o main.o
	gcc $^ -Ljpeg-6b-steg -l:libjpeg.a -o $@

%.o: %.c
	gcc -c $< -o $@

.PHONY: clean mrproper
clean:
	rm main.o
	rm d5.o
	rm arc.o
	rm iterator.o
mrproper: clean
	rm unoutguess
