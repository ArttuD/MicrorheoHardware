#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

class PID
{
public:
    PID(double kp, double ki, double kd, double fc, double Ts, double maxOutput);
    double update(double* reference, double* meas_y);
    double calcAlphaEMA(double fn);

private:
    double kp, ji, kd, ki, alpha, Ts, maxOutput;
    double integral = 0;
    double oldEf = 0;

};
