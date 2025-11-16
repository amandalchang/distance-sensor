//
// Convert cleaned RSSI value to distance in meters using logarithmic regression
//
#include <math.h>
#include <stdio.h>

// avoids using pow(x, 2) which is less clean and slower
static inline double sqr(double d) { return d*d; }

int linreg(int num_dists, const double x[], const double rssi[], double* m, double* b, double* r) {
    double sumx = 0.0;
    double sumx2 = 0.0;
    double sumxy = 0.0;
    double sumy = 0.0;
    double sumy2 = 0.0;

    for (int i = 0; i < num_dists; i++) {
        sumx  += x[i];
        sumx2 += sqr(x[i]);
        sumxy += x[i] * rssi[i];
        sumy  += rssi[i];
        sumy2 += sqr(rssi[i]);
    }

    double denom = (num_dists * sumx2 - sqr(sumx));
    if (denom == 0.0) {
        *m = 0.0;
        *b = 0.0;
        if (r) *r = 0.0;
        return 1;
    }

    *m = (num_dists * sumxy - sumx * sumy) / denom;
    *b = (sumy * sumx2 - sumx * sumxy) / denom;

    if (r != NULL) {
        *r = (sumxy - sumx * sumy / num_dists) /
             sqrt((sumx2 - sqr(sumx) / num_dists) *
                  (sumy2 - sqr(sumy) / num_dists));
    }

    return 0;
}

// validatation of logarithmic regression model
void rssi_to_dist(const int num_dists, const double rssi[], double m, double b, double distances[]) {
    for (int i = 0; i < num_dists; i++) {
        distances[i] = pow(10, (rssi[i] - b) / m);
    };
}


int main(void) {
    const int num_dists = 5; // number of measurements
    double m = 0; // stores slope
    double b = 0; // stores y-intercept
    double r = 0;
    double log_distances[num_dists]; // storage for log distances
    double distances[num_dists]; // fills in calculated distances

    // assuming 1 meter apart from each measurement
    for (int i = 0; i < num_dists; i++) {
        log_distances[i] = log10(i+1);
        printf("log %i is %.3f\n", i+1, log_distances[i]);
    };
    // initializing sample rssi values
    const double rssi[5] = {5.000, 5.602, 5.954, 6.204, 6.398};
    linreg(num_dists, log_distances, rssi, &m, &b, &r);
    printf("rssi = %.3f + %.3f*log_dists\nr = %.3f\n", b, m, r);

    // convert rssi to distances using calculated equation
    rssi_to_dist(num_dists, rssi, m, b, distances);
    for (int i = 0; i < num_dists; i+=1) {
        printf("The original distance was %i and the calculated was %0.4f\n", i+1, distances[i]);
    };

    // save calculated logarithmic parameters
    FILE *f = fopen("rssi_params.txt", "w");
    fprintf(f, "%0.4f %0.4f\n", m, b);
    fclose(f);
}
