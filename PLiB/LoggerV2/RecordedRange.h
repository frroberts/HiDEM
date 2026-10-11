//
// Created by Fredrik Robertsen on 5/4/26.
//

#ifndef PHELL_RECORDEDRANGE_H
#define PHELL_RECORDEDRANGE_H

class RecordedRange {
    //
public:
    std::chrono::duration<double> cumDuration = std::chrono::duration<double>::zero();
    std::chrono::duration<double> minDuration = std::chrono::duration<double>::max();
    std::chrono::duration<double> maxDuration = std::chrono::duration<double>::min();
    int count = 0;
    int level;
    int createdOrder;
};



#endif //PHELL_RECORDEDRANGE_H