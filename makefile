compile:
	python3 generate_test_case.py
	gcc helper.c -o helper
	gcc solution.c -o solution
	./helper 1
	rm solution
	rm helper
	rm *.txt
	rm *.bin
