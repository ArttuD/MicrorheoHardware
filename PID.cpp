
#include "Header.hpp"
#include <iostream>
#include <cmath>
#include <math.h>
#include <stdio.h>

# define M_PI  


PID::PID(double kp, double ki, double kd, double fc, double Ts, double maxOutput)
	: kp(kp), ki(ki), kd(kd), alpha(calcAlphaEMA(fc* Ts)), Ts(Ts), maxOutput(maxOutput) {}

double PID::update(double* reference, double* meas_y)
{
	double error = *reference - *meas_y; //True value and setpoint
	// float ef = alpha * error + (1 - alpha) * oldEf; // filtered error
	// float derivative = (ef - oldEf) / 1.0; // filtered derivative
	// float newIntegal = integral + error * 1.0; //Integral

	//PID
	double control_u = kp * error; // + ki * integral + kd * derivative;

	//  std::cout << kp << "/" << ki << "/" << kd << "/" << alpha << "/" << Ts << "/" << maxOutput << std::endl;
	//  std::cout << error << "/" << ef << "/" << derivative << "/" << newIntegal << "/" << oldEf << std::endl;
	//clamp to prevent too large outputs

	if (control_u > maxOutput) {
		control_u = maxOutput;
	}
	else if (control_u < -maxOutput) {
		control_u = -maxOutput;
	}
	// else {
		//  integral = newIntegal;
	// }

	//  oldEf = ef;

	return control_u; //return control signal
}

double PID::calcAlphaEMA(double fn) {
	if (fn <= 0)
		return (double)1;

	double c = std::cos(2 * double(M_PI) * fn);
	return c - 1 + std::sqrt(c * c - 4 * c + 3);
}
