#ifndef COUNTERS_H
#define COUNTERS_H

#include <map>
#include <unordered_map>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include "read.h"
#include "typedefs.h"
#include <unistd.h>

class Counters {
public:
    uint64_t TotalFragmentsInput = 0;
    uint64_t PE_In = 0;
    uint64_t SE_In = 0;
    uint64_t TotalFragmentsOutput = 0;
    uint64_t PE_Out = 0;
    uint64_t SE_Out = 0;

    std::vector<Label> labels;
    Counters() {
        labels.push_back(std::forward_as_tuple("TotalFragmentsInput", TotalFragmentsInput));
        labels.push_back(std::forward_as_tuple("PE_In", PE_In));
        labels.push_back(std::forward_as_tuple("SE_In", SE_In));
        labels.push_back(std::forward_as_tuple("TotalFragmentsOutput", TotalFragmentsOutput));
        labels.push_back(std::forward_as_tuple("PE_Out", PE_Out));
        labels.push_back(std::forward_as_tuple("SE_Out", SE_Out));
    }
    virtual void input(const PairedEndRead &read) {
        ++TotalFragmentsInput;
        ++PE_In; 
    }

    virtual void input(const SingleEndRead &read) {
        ++TotalFragmentsInput;
        ++SE_In; 
    }

    virtual void input(const ReadBase &read) {
        const PairedEndRead *per = dynamic_cast<const PairedEndRead *>(&read);
        if (per) {
            input(*per);
        } else {
            const SingleEndRead *ser = dynamic_cast<const SingleEndRead *>(&read);
            if (ser) {
                input(*ser); 
            } else {
                throw std::runtime_error("In utils.h output: read type not valid");
            }
        } 
    }

    virtual void output(PairedEndRead &read, bool no_orhpans = false) {
        ++TotalFragmentsOutput;
        ++PE_Out;
    }


    virtual void output(SingleEndRead &read) {
        ++TotalFragmentsOutput;
        ++SE_Out;
    }

    virtual void output(ReadBase &read) {
        PairedEndRead *per = dynamic_cast<PairedEndRead *>(&read);
        if (per) {
            output(*per);
        } else {
            SingleEndRead *ser = dynamic_cast<SingleEndRead *>(&read);
            if (ser) {
                output(*ser); 
            } else {
                throw std::runtime_error("In utils.h output: read type not valid");
            }
        } 
    }

    virtual void write_out(const std::string &statsFile, bool appendStats, std::string program_name, std::string notes) {
        
        std::ifstream testEnd(statsFile);
        int end = testEnd.peek();
        testEnd.close();
        
        std::fstream outStats;
        
        if (appendStats && end != -1) {
            //outStats.open(statsFile, std::ofstream::out | std::ofstream::app); //overwritte
            outStats.open(statsFile, std::ios::in | std::ios::out); //overwritte
            outStats.seekp(-6, std::ios::end );
            outStats << "  }, \"" << program_name << "_" << getpid()  << "\": {\n";
        } else {
            //outStats.open(statsFile, std::ofstream::out); //overwritte
            outStats.open(statsFile, std::ios::out | std::ios::trunc); //overwrite
            outStats << "{ \"" << program_name << "_" << getpid() <<  "\": {\n";
        }
        outStats << "    \"Notes\": \"" << notes << "\"";

        write_labels(outStats);
        
        outStats << "\n  }\n}\n";
        outStats.flush();
    }

    virtual const void write_labels(std::fstream& outStats) {
        for (const auto &label : labels) {
            outStats << ",\n"; //make sure json format is kept
            outStats << "    \"" << std::get<0>(label) << "\": " << std::get<1>(label);
        }
    }

};


class OverlappingCounters : public Counters {

public:
    std::vector<unsigned long long int> insertLength;
    uint64_t sins = 0;
    uint64_t mins = 0;
    uint64_t lins = 0;
    uint64_t SE_Discard = 0;
    uint64_t PE_Discard = 0;
    uint64_t Adapter_BpTrim = 0;

    OverlappingCounters() {
        labels.push_back(std::forward_as_tuple("sins", sins));
        labels.push_back(std::forward_as_tuple("mins", mins));
        labels.push_back(std::forward_as_tuple("lins", lins));
        labels.push_back(std::forward_as_tuple("SE_Discard", SE_Discard));
        labels.push_back(std::forward_as_tuple("PE_Discard", PE_Discard));
        labels.push_back(std::forward_as_tuple("Adapter_BpTrim", Adapter_BpTrim));

        insertLength.resize(1);
    }

    virtual void output(SingleEndRead &ser)  {
        if (ser.non_const_read_one().getDiscard()) {
            ++SE_Discard;
        } else {
            ++TotalFragmentsOutput;
            ++SE_Out;
        }
    }

    virtual void output(PairedEndRead &per)  {
        if (per.non_const_read_one().getDiscard()) {
            ++PE_Discard;
        } else {
            ++lins;
            ++TotalFragmentsOutput;
            ++PE_Out;
        }
    }

    virtual void output(SingleEndRead &ser, unsigned int origLength)  {
        Read &one = ser.non_const_read_one();
        if (!one.getDiscard()) {
            if (one.getLength() < origLength) {
                ++sins; //adapters must be had (short insert)
                Adapter_BpTrim += (origLength - one.getLength());
            } else {
                ++mins; //must be a long insert
            }
            if ( one.getLength() + 1 > insertLength.size() ) {
                insertLength.resize(one.getLength() + 1);
            }
            ++insertLength[one.getLength()];
            ++SE_Out;            
            ++TotalFragmentsOutput;
        } else {
            ++PE_Discard; // originated as a PE read
        }
    }

    virtual void output(PairedEndRead &per, unsigned int overlapped) {
        //test lin or sin
        Read &one = per.non_const_read_one();
        Read &two = per.non_const_read_two();

        if (!one.getDiscard() && !two.getDiscard() ) {
            if (overlapped) {
                if (one.getLength() > overlapped || two.getLength() > overlapped ) {
                    ++sins; //adapters must be had (short insert)
                    Adapter_BpTrim += std::max((one.getLength() - one.getLengthTrue()),(two.getLength() - two.getLengthTrue()));

                } else {
                    ++mins; //must be a long insert
                }
                if ( overlapped + 1 > insertLength.size() ) {
                    insertLength.resize(overlapped + 1);
                }
                ++insertLength[overlapped];
                
            } else {
                ++lins; //lin
            }
            ++PE_Out;
            ++TotalFragmentsOutput;
        } else {
            ++PE_Discard;
        }
    }

    virtual void write_out(const std::string &statsFile, bool appendStats, std::string program_name, std::string notes) {

        std::ifstream testEnd(statsFile);
        int end = testEnd.peek();
        testEnd.close();

        std::fstream outStats;

        if (appendStats && end != -1) {
            outStats.open(statsFile, std::ios::in | std::ios::out); //overwritte
            outStats.seekp(-6, std::ios::end );
            outStats << "  }, \"" << program_name << "_" << getpid()  << "\": {\n";
        } else {
            outStats.open(statsFile, std::ios::out | std::ios::trunc); //overwritt
            outStats << "{ \"" << program_name << "_" << getpid() <<  "\": {\n";
        }

        outStats << "    \"Notes\": \"" << notes << "\"";

        write_labels(outStats);

        // embed instertLength (histogram) in sub json vector
        outStats << ",\n"; //make sure json format is kept
        outStats << "    \"histogram\": [";
        outStats << "[" << 1 << "," << insertLength[1] << "]";  // first, so as to keep the json comma convention

        for (size_t i = 2; i < insertLength.size(); ++i) {
            outStats << ", [" << i << "," << insertLength[i] << "]"; //make sure json format is kept
        }
        outStats << "]"; // finish off histogram
        outStats << "\n  }\n}\n";
        outStats.flush();
    }
};

class PhixCounters : public Counters {

public:
    uint64_t PE_hits = 0;
    uint64_t SE_hits = 0;
    uint64_t Inverse = 0;

    PhixCounters() {
        labels.push_back(std::forward_as_tuple("PE_hits", PE_hits));
        labels.push_back(std::forward_as_tuple("SE_hits", SE_hits));
        labels.push_back(std::forward_as_tuple("Inverse", Inverse));
    }

    void set_inverse() {
        Inverse = 1;
    }
    void inc_SE_hits() {
        ++SE_hits;
    }
    void inc_PE_hits() {
        ++PE_hits;
    }
};

class TrimmingCounters : public Counters {

public: 
    uint64_t R1_Left_Trim = 0;
    uint64_t R1_Right_Trim = 0;
    uint64_t R2_Left_Trim = 0;
    uint64_t R2_Right_Trim = 0;
    uint64_t SE_Right_Trim = 0;
    uint64_t SE_Left_Trim = 0;
    
    uint64_t R1_Discarded = 0;
    uint64_t R2_Discarded = 0;
    uint64_t SE_Discarded = 0;
    uint64_t PE_Discarded = 0;

    TrimmingCounters() {
        labels.push_back(std::forward_as_tuple("R1_Left_Trim", R1_Left_Trim));
        labels.push_back(std::forward_as_tuple("R1_Right_Trim", R1_Right_Trim));
        labels.push_back(std::forward_as_tuple("R2_Left_Trim", R2_Left_Trim));
        labels.push_back(std::forward_as_tuple("R2_Right_Trim", R2_Right_Trim));
        labels.push_back(std::forward_as_tuple("SE_Right_Trimm", SE_Right_Trim));
        labels.push_back(std::forward_as_tuple("SE_Left_Trimm", SE_Left_Trim));
        labels.push_back(std::forward_as_tuple("R1_Discarded", R1_Discarded));
        labels.push_back(std::forward_as_tuple("R2_Discarded", R2_Discarded));
        labels.push_back(std::forward_as_tuple("SE_Discarded", SE_Discarded));
        labels.push_back(std::forward_as_tuple("PE_Discarded", PE_Discarded));

    }

    void R1_stats(Read &one) {
        R1_Left_Trim += one.getLTrim();
        R1_Right_Trim += one.getRTrim();
    }

    void R2_stats(Read &two) {
        R2_Left_Trim += two.getLTrim();
        R2_Right_Trim += two.getRTrim();
    }

    void SE_stats(Read &se) {
        SE_Left_Trim += se.getLTrim();
        SE_Right_Trim += se.getRTrim();
    }

    void output(PairedEndRead &per, bool no_orphans = false) {
        Read &one = per.non_const_read_one();
        Read &two = per.non_const_read_two();
        if (!one.getDiscard() && !two.getDiscard()) {
            ++TotalFragmentsOutput;
            ++PE_Out;
            R1_stats(one);
            R2_stats(two);
        } else if (!one.getDiscard() && !no_orphans) { //if stranded RC
            ++TotalFragmentsOutput;
            ++SE_Out;
            ++R2_Discarded;
            SE_stats(one);
        } else if (!two.getDiscard() && !no_orphans) { // Will never be RC
            ++TotalFragmentsOutput;
            ++SE_Out;
            ++R1_Discarded;
            SE_stats(two);
        } else {
            ++PE_Discarded;
        }
    }

    void output(SingleEndRead &ser) {
        Read &one = ser.non_const_read_one();
        if (!one.getDiscard()) {
            ++TotalFragmentsOutput;
            ++SE_Out;
            SE_stats(one);
        } else {
            ++SE_Discarded;
        }
    }
    
};

class StatsCounters : public Counters {

public:
    uint64_t A = 0;
    uint64_t T = 0;
    uint64_t C = 0;
    uint64_t G = 0;
    uint64_t N = 0;
    uint64_t R1_BpLen = 0;
    uint64_t R2_BpLen = 0;
    uint64_t SE_BpLen = 0;
    uint64_t R1_bQ30 = 0;
    uint64_t R2_bQ30 = 0;
    uint64_t SE_bQ30 = 0;

    StatsCounters () {
        labels.push_back(std::forward_as_tuple("R1_BpLen", R1_BpLen));
        labels.push_back(std::forward_as_tuple("R2_BpLen", R2_BpLen));
        labels.push_back(std::forward_as_tuple("SE_BpLen", SE_BpLen));
        labels.push_back(std::forward_as_tuple("R1_bQ30", R1_bQ30));
        labels.push_back(std::forward_as_tuple("R2_bQ30", R2_bQ30));
        labels.push_back(std::forward_as_tuple("SE_bQ30", SE_bQ30));
    };


    void read_stats(Read &r) {
        for (auto bp : r.get_seq()) {
            switch (bp) {
                case 'A':
                    ++A;
                    break;
                case 'T':
                    ++T;
                    break;
                case 'C':
                    ++C;
                    break;
                case 'G':
                    ++G;
                    break;
                case 'N':
                    ++N;
                    break;
                default:
                    throw std::runtime_error("Unknown bp in stats counter");
            }
        }
    }
 
    void q_stats(Read &r, unsigned long long int &val) {
    }
 
    void output(PairedEndRead &per) {
        Counters::output(per);
        Read &one = per.non_const_read_one();
        Read &two = per.non_const_read_two();
        read_stats(one);
        read_stats(two);
        int r1_q30bases=0;
        for (auto q : one.get_qual()) {
            r1_q30bases += (q - 33) >= 30;
        }
        int r2_q30bases=0;
        for (auto q : two.get_qual()) {
            r2_q30bases += (q - 33) >= 30;
        }
        R1_bQ30 += r1_q30bases;
        R2_bQ30 += r2_q30bases;
        R1_BpLen += one.getLength();
        R2_BpLen += two.getLength();
    }
    
    void output(SingleEndRead &ser) {
        Counters::output(ser);
        Read &one = ser.non_const_read_one();
        read_stats(one);
        int q30bases=0;
        for (auto q : one.get_qual()) {
            q30bases += (q - 33) >= 30;
        }
        SE_bQ30 += q30bases;
        SE_BpLen += one.getLength();
    }

    virtual void write_out(const std::string &statsFile, bool appendStats, std::string program_name, std::string notes) {
        
        std::ifstream testEnd(statsFile);
        int end = testEnd.peek();
        testEnd.close();
        
        std::fstream outStats;
        
        if (appendStats && end != -1) {
            //outStats.open(statsFile, std::ofstream::out | std::ofstream::app); //overwritte
            outStats.open(statsFile, std::ios::in | std::ios::out); //overwritte
            outStats.seekp(-6, std::ios::end );
            outStats << "  }, \"" << program_name << "_" << getpid()  << "\": {\n";
        } else {
            //outStats.open(statsFile, std::ofstream::out); //overwritte
            outStats.open(statsFile, std::ios::out | std::ios::trunc); //overwrite
            outStats << "{ \"" << program_name << "_" << getpid() <<  "\": {\n";
        }
        outStats << "    \"Notes\": \"" << notes << "\"";

        write_labels(outStats);
        
        // embed base composition as sub json vector
        outStats << ",\n"; //make sure json format is kept
        outStats << "    \"Base_Composition\": {\n";
        outStats << "        \"" << 'A' << "\": " << A << ",\n";
        outStats << "        \"" << 'T' << "\": " << T << ",\n";
        outStats << "        \"" << 'G' << "\": " << G << ",\n";
        outStats << "        \"" << 'C' << "\": " << C << ",\n";
        outStats << "        \"" << 'N' << "\": " << N << "\n";
        outStats << "    }"; // finish off histogram
        outStats << "\n  }\n}\n";
        outStats.flush();
    }
 
};

class SuperDeduperCounters : public Counters {
public:
    uint64_t Duplicate = 0;
    uint64_t Ignored = 0;

    SuperDeduperCounters() {
        labels.push_back(std::forward_as_tuple("Duplicate", Duplicate));
        labels.push_back(std::forward_as_tuple("Ignored", Ignored));
    }

    void increment_replace() {
        ++Duplicate;
    }

    void increment_ignored() {
        ++Ignored;
    }
};


#endif
