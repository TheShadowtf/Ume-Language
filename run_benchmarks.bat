@echo off
echo ================================================================
echo           Running Ume Language vs C++ Benchmark Suite            
echo ================================================================
echo.

python benchmarks\runner\run_benchmarks.py

echo.
echo Benchmark run completed. Reports saved in benchmarks\results\
pause
