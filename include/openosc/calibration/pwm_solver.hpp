#pragma once
#include <cstdint>

namespace openosc::calibration {
// One channel's search for the offset PWM compare at which a grounded input reads its expected
// zero code: error = measured mean - expected, in codes, at the compare under measurement.
//   - The step is error / slope with the secant of the last two measurements when that agrees
//     with the factory slope (PwmGeometry: codes per count, its sign the row's) in sign and lies
//     within a factor of four of it; without one, HALF of error / factory slope. For a true slope
//     r x the factory one (r in 1/4..4) the error after such a step is error x (1 - r/2), never
//     larger than before: every measurement stays within the start's error, so within the
//     sampler's MaximumShiftCodes, and the next step has a secant. From +-40 codes a model with
//     slope ratio 1/4..4 converges in at most six steps (three at ratio 1/2..2); with +-0.1 code
//     of measurement noise at ratio 1/4 the tail reaches twelve: MaximumIterations.
//   - The errors' signs bracket the answer: a step leaving the bracket takes its middle.
//   - Done at |error| <= ToleranceCodes. At the compare resolution only when the rest is what one
//     count explains: neighbouring compares on either side whose errors differ by no more than
//     four factory slopes (+ the tolerance), or a step under half a count with |error| within half
//     the slope in use (+ the tolerance). Otherwise the PWM does not do what the table says.
//   - Fails: a needed step past the compare's ends (0, 9999: the rail); a step the factory slope
//     makes worth 4 codes (so a PWM at 1/4 of it moves a code) that moves the error by less than
//     SignificantCodes, twice (no response); a significant secant of the
//     wrong sign twice (not monotonic, a moving input); neighbours too far apart; or
//     MaximumIterations without reaching 2 x ToleranceCodes.
class PwmSolver {
public:
    enum class Step { Continue,Converged,Failed };
    static constexpr double ToleranceCodes=.25,SignificantCodes=.5,ReachCodes=80; // ReachCodes: no step moves further.
    static constexpr unsigned MaximumIterations=12;
    void begin(uint16_t start,double codesPerCount);
    uint16_t compare() const { return compare_; } // The compare to measure next.
    Step update(double error);
    uint16_t best() const { return best_; }       // The compare whose error was smallest.
    double bestError() const { return bestError_; }
    unsigned iterations() const { return iterations_; }
    bool converged() const { return converged_; }
private:
    Step converge(uint16_t compare);
    double slope_=1,previousError_=0,bestError_=0,lowError_=0,highError_=0;
    uint16_t compare_=0,previous_=0,best_=0,low_=0,high_=0; // low_/high_: compares with error < 0 / > 0.
    unsigned iterations_=0,wrongSign_=0,unresponsive_=0;
    bool havePrevious_=false,haveLow_=false,haveHigh_=false,converged_=false;
};
} // namespace openosc::calibration
