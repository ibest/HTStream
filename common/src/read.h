#ifndef READ_H
#define READ_H

#include <boost/dynamic_bitset.hpp>

typedef boost::dynamic_bitset<> BitSet;

class Noop {
public:
    int operator()(int x) const { return x; }
};

class ReadBase {
public:
    virtual ~ReadBase() {}
    virtual boost::dynamic_bitset<> get_key(size_t start, size_t length) = 0;

    template <typename functor = Noop>
    static boost::dynamic_bitset<> str_to_bit(const std::string& StrKey, functor const & transform = Noop()) {
          // converts a string to a 2bit representation: A:00, T:11, C:01, G:10
        // ~ will then convert to the complimentary bp
        boost::dynamic_bitset<> bit(2 * StrKey.length());
        size_t i = (2 * StrKey.length()) - 1;
        
        for(const char &c : StrKey){
            //  non branching conversion
            bit[i-1] = transform(!!((c + 10) & ( 1 << 2)));
            bit[i] = transform(!!((c + 10) & ( 1 << 4)));
            i -= 2;
        }
        return bit;
    }
    
    static std::string bit_to_str(const boost::dynamic_bitset<> &bits);
    static BitSet reverse_complement(const std::string& str, int start, int length);
    virtual double avg_q_score() = 0;
};

class Read {
private:
    std::string seq;
    std::string qual;
    std::string id;

public:
    Read(const std::string& seq_, const std::string& qual_, const std::string& id_) :
        seq(seq_), qual(qual_), id(id_) { }
    Read subread(size_t start, size_t length);
    std::string subseq(size_t start, size_t length);
    const std::string& get_seq() const { return seq; }
    const std::string& get_qual() const { return qual; }
    const std::string& get_id() const { return id; }
};

class PairedEndRead: public ReadBase {
private:
    Read one;
    Read two;
public:
    PairedEndRead(const Read& one_, const Read& two_) :
        one(one_), two(two_) { }
    boost::dynamic_bitset<> get_key(size_t start, size_t length);
    const Read& get_read_one() const { return one; }
    const Read& get_read_two() const { return two; }
    double avg_q_score();
};

class SingleEndRead: public ReadBase {
private:
    Read one;
public:
    SingleEndRead(const Read& one_) :
        one(one_) { }
    boost::dynamic_bitset<> get_key(size_t start, size_t length);
    const Read& get_read() const { return one; }
    double avg_q_score();
};

#endif