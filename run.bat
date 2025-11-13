rm ./hotdog_manager
rm -rf test/
rm ./log.txt
mkdir test
gcc hotdog_manager.c -o hotdog_manager -lpthread
echo error_tests
echo "# Non-integer or junk values" >> test/test_err.txt
echo "./hotdog_manager" >> test/test_err.txt
./hotdog_manager 2>> test/test_err.txt
echo "./hotdog_manager 10" >> test/test_err.txt
./hotdog_manager 10 2>> test/test_err.txt
echo "./hotdog_manager 10 3" >> test/test_err.txt
./hotdog_manager 10 3 2>> test/test_err.txt
echo "./hotdog_manager 10 3 2" >> test/test_err.txt
./hotdog_manager 10 3 2 2>> test/test_err.txt
echo "./hotdog_manager 10 3 2 2 extra" >> test/test_err.txt
./hotdog_manager 10 3 2 2 extra 2>> test/test_err.txt
echo "" >> test/test_err.txt
echo "# Empty / whitespace-only arguments" >> test/test_err.txt
echo "./hotdog_manager '' 3 2 2 " >> test/test_err.txt
./hotdog_manager "" 3 2 2 2>> test/test_err.txt
echo "./hotdog_manager '   ' 3 2 2 " >> test/test_err.txt
./hotdog_manager "   " 3 2 2  2>> test/test_err.txt
echo "" >> test/test_err.txt
echo "# Non-integer or junk values" >> test/test_err.txt
echo "./hotdog_manager abc 3 2 2" >> test/test_err.txt
./hotdog_manager abc 3 2 2  2>> test/test_err.txt
echo "./hotdog_manager 10 3x 2 2" >> test/test_err.txt
./hotdog_manager 10 3x 2 2 2>> test/test_err.txt
echo "./hotdog_manager 10 3 2.5 2" >> test/test_err.txt
./hotdog_manager 10 3 2.5 2 2>> test/test_err.txt
echo "./hotdog_manager 10 2 2.0 2" >> test/test_err.txt
./hotdog_manager 10 3 2 2.0 2>> test/test_err.txt
echo "./hotdog_manager 10xyz 3 2 2" >> test/test_err.txt
./hotdog_manager 10xyz 3 2 2 2>> test/test_err.txt
echo "" >> test/test_err.txt
echo "# Negative or zero values" >> test/test_err.txt
echo "./hotdog_manager 0 3 2 2" >> test/test_err.txt
./hotdog_manager 0 3 2 2 2>> test/test_err.txt
echo "./hotdog_manager 10 0 2 2" >> test/test_err.txt
./hotdog_manager 10 0 2 2 2>> test/test_err.txt
echo "./hotdog_manager 10 3 0 2" >> test/test_err.txt
./hotdog_manager 10 3 0 2 2>> test/test_err.txt
echo "./hotdog_manager 10 3 2 0" >> test/test_err.txt
./hotdog_manager 10 3 2 0 2>> test/test_err.txt
echo "" >> test/test_err.txt
echo "# Out of range / invalid relationships" >> test/test_err.txt
echo "./hotdog_manager -5 3 2 2" >> test/test_err.txt
./hotdog_manager -5 3 2 2 2>> test/test_err.txt
echo "./hotdog_manager 10 -1 2 2" >> test/test_err.txt
./hotdog_manager 10 -1 2 2 2>> test/test_err.txt
echo "./hotdog_manager 10 3 -2 2" >> test/test_err.txt
./hotdog_manager 10 3 -2 2 2>> test/test_err.txt
echo "./hotdog_manager 10 3 31 2" >> test/test_err.txt
./hotdog_manager 10 3 31 2 2>> test/test_err.txt
echo "./hotdog_manager 10 3 2 31" >> test/test_err.txt
./hotdog_manager 10 3 2 31 2>> test/test_err.txt
echo "./hotdog_manager 5 5 2 2" >> test/test_err.txt
./hotdog_manager 5 5 2 2 2>> test/test_err.txt
echo "./hotdog_manager 5 6 2 2" >> test/test_err.txt
./hotdog_manager 5 6 2 2 2>> test/test_err.txt

echo test1
echo "./hotdog_manager 10 3 2 2" >> test/test1.log
./hotdog_manager 10 3 2 2 >> test/test1.log
cp log.txt test/test1.txt
rm log.txt
echo test2
echo "./hotdog_manager 30 5 5 2" >> test/test2.log
./hotdog_manager 30 5 5 2  >> test/test2.log
cp log.txt test/test2.txt
rm log.txt
echo test3
echo "./hotdog_manager 50 5 2 5" >> test/test3.log
./hotdog_manager 50 5 2 5  >> test/test3.log
cp log.txt test/test3.txt
rm log.txt
echo test4
echo "./hotdog_manager 50 10 3 10" >> test/test4.log
./hotdog_manager 50 10 3 10  >> test/test4.log
cp log.txt test/test4.txt
rm log.txt
echo test5
echo "./hotdog_manager 50 10 10 3" >> test/test5.log
./hotdog_manager 50 10 10 3  >> test/test5.log
cp log.txt test/test5.txt
rm log.txt
echo test6
echo "./hotdog_manager 100 20 30 30" >> test/test6.log
./hotdog_manager 100 20 30 30  >> test/test6.log
cp log.txt test/test6.txt
rm log.txt