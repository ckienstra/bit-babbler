//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2021,  Ron <ron@debian.org>
//
//  With much kudos to John "Random" Walker for the public domain ENT suite
//  of tests, which the implementation below doesn't actually take any code
//  from, but which was shamelessly pillaged for ideas about which metrics
//  can be the most valuable indicators of randomness being Not Quite Right.
//  The original code for ENT can be found at <http://www.fourmilab.ch/random>

#ifndef _BB_QA_H
#define _BB_QA_H

#include <bit-babbler/json.h>
#include <bit-babbler/chisq.h>
#include <bit-babbler/math.h>
#include <bit-babbler/aligned_recast.h>

#include <vector>
#include <algorithm>
#include <cfloat>


namespace BitB
{

    // This one probably doesn't really belong here, but everything that needs it
    // includes this header, and putting it alone in its own would be overkill.
    // It might move somewhere more appropriate if we ever have such a thing.
    static inline size_t FoldBytes( uint8_t *buf, size_t len, unsigned folds )
    { //{{{

        if( len & ((1u << folds) - 1) )
            throw Error( _("FoldBytes: length %zu cannot fold %u times"), len, folds );

        for( ; folds; --folds )
        {
            len >>= 1;

            for( size_t i = 0; i < len; ++i )
                buf[i] ^= buf[len + i];
        }

        return len;

    } //}}}


  namespace QA
  {

    // Implements the metrics from John "Random" Walker's ENT suite, as well
    // as an estimation of the min-entropy using the method that is described
    // in section 9.2 of the NIST SP 800-90B 2012 draft.  These tests provide
    // a more sensitive measure of the statistical quality of the RNG, though
    // not as good as a more comprehensive suite like dieharder, but are also
    // still fast to run, so we can use them for continuous reporting on the
    // generated bitstream.  We can also use them to provide both short and
    // long term measurements, since they are more accurate on larger sets of
    // data, but provide good enough results on smaller sets to make changes
    // in the statistical quality of the data become quickly evident.
    template< typename T >
    class Ent
    { //{{{
    public:

        static const size_t     NBITS = sizeof(T) * 8;
        static const size_t     NBINS = 1u << NBITS;


        enum DataSet
        { //{{{

            // MIN and MAX are slightly fuzzy concepts here, but BEST and WORST
            // wouldn't actually fit all the data types much better either ...
            // In general MIN is minimum value or error, and MAX is maximum
            // value or error, as is most appropriate for each data point.
            // Specifically:
            //
            // For entropy, MAX is the best value seen and MIN is the worst.
            //
            // For Chi^2 there is no single best value - an ideal result will
            // wander about in the expected range, but shouldn't wander above
            // or below it too far or for too long.  Staying fixed exactly in
            // the middle of it all the time would be suspiciously too uniform.
            //
            // For Mean, Autocorrelation, and Pi, MIN is the best value seen
            // (the least deviation above or below the ideal value) and MAX is
            // the result with the largest error (whether positive or negative).

            CURRENT         = 0,
            MIN             = 1,
            MAX             = 2,
            DATASET_MAX

        }; //}}}

        static const char *DataSetName( DataSet s )
        { //{{{

            switch( s )
            {
                case CURRENT:       return "Current";
                case MIN:           return "Min";
                case MAX:           return "Max";
                case DATASET_MAX:   break;
            }
            return "Unknown DataSet";

        } //}}}


        struct Limits
        { //{{{

            size_t      long_minsamples;

            double      long_entropy;
            double      short_entropy;

            double      long_chisq_min;
            double      long_chisq_max;
            double      short_chisq_min;
            double      short_chisq_max;

            double      long_mean_min;
            double      long_mean_max;
            double      short_mean_min;
            double      short_mean_max;

            double      long_pi;
            double      short_pi;

            double      long_corr;
            double      short_corr;

            double      long_minentropy;
            double      short_minentropy;

            unsigned    recovery_blocks;

        }; //}}}

        struct Result
        { //{{{

            double  entropy;
            double  chisq;
            double  mean;
            double  pi;
            double  corr;
            double  minentropy;


            Result()
                : entropy( 0.0 )
                , chisq( 0.0 )
                , mean( 0.0 )
                , pi( 0.0 )
                , corr( 0.0 )
                , minentropy( 0.0 )
            {}

            Result( DataSet set )
            {
                clear( set );
            }

            Result( const Json::Data::Handle &result )
                : entropy(    result["Entropy"] )
                , chisq(      result["Chisq"] )
                , mean(       result["Mean"] )
                , pi(         result["Pi"] )
                , corr(       result["Autocorr"] )
                , minentropy( result["MinEntropy"] )
            {}


            void clear( DataSet set = CURRENT )
            { //{{{

                switch( set )
                {
                    case MIN:
                        entropy     = DBL_MAX;
                        chisq       = DBL_MAX;
                        mean        = DBL_MAX;
                        pi          = DBL_MAX;
                        corr        = DBL_MAX;
                        minentropy  = DBL_MAX;
                        break;

                    case MAX:
                        entropy     = -DBL_MAX;
                        chisq       = -DBL_MAX;
                        mean        = (1u << (NBITS - 1)) - 0.5;
                        pi          = M_PI;
                        corr        = 0.0;
                        minentropy  = -DBL_MAX;
                        break;

                    case CURRENT:
                    case DATASET_MAX:
                        entropy     = 0.0;
                        chisq       = 0.0;
                        mean        = 0.0;
                        pi          = 0.0;
                        corr        = 0.0;
                        minentropy  = 0.0;
                        break;
                }

            } //}}}


            bool operator!=( const Result &r ) const
            { //{{{

                return r.entropy < entropy       || r.entropy > entropy
                    || r.chisq < chisq           || r.chisq > chisq
                    || r.mean < mean             || r.mean > mean
                    || r.pi < pi                 || r.pi > pi
                    || r.corr < corr             || r.corr > corr
                    || r.minentropy < minentropy || r.minentropy > minentropy;

            } //}}}

            bool operator==( const Result &r ) const
            {
                return ! (*this != r);
            }


            double ChisqProb() const
            {
                return pochisq( chisq, NBINS - 1 );
            }

            double PiError() const
            { //{{{

                // If this is queried when cleared to MIN or MAX then it will
                // evaluate to +/- inf since pe will overflow.  That's not a
                // very useful (or true) answer here, expecially since we pass
                // this into JSON streams where 'inf' is not a valid number.
                // So just return +/- DBL_MAX in those cases.
                double  pe = 100.0 * (pi - M_PI) / M_PI;

                if( std::isfinite(pe) )
                    return pe;

                if( pe < 0.0 )
                    return -DBL_MAX;

                return DBL_MAX;

            } //}}}


            std::string Report() const
            { //{{{

                return stringprintf( "Hs %f, Hm %f, Mean %f, Corr % .8f,"
                                        " π %.8f (% .5f), χ² %f (%.2f)",
                                     entropy, minentropy, mean, corr,
                                     pi, PiError(), chisq, ChisqProb() );
            } //}}}

            std::string AsJSON() const
            { //{{{

                return stringprintf( "{"
                                      "\"Entropy\":%f"
                                     ",\"Chisq\":%f"
                                     ",\"Chisq-p\":%f"
                                     ",\"Mean\":%f"
                                     ",\"Pi\":%f"
                                     ",\"Pi-error\":%f"
                                     ",\"Autocorr\":%f"
                                     ",\"MinEntropy\":%f"
                                     "}",
                                     entropy, chisq, ChisqProb(), mean,
                                     pi, PiError(), corr, minentropy );
            } //}}}

        }; //}}}

        struct Fail
        { //{{{

            size_t  tested;

            size_t  entropy;
            size_t  chisq;
            size_t  mean;
            size_t  pi;
            size_t  corr;
            size_t  minentropy;


            Fail()
                : tested( 0 )
                , entropy( 0 )
                , chisq( 0 )
                , mean( 0 )
                , pi( 0 )
                , corr( 0 )
                , minentropy( 0 )
            {}

            Fail( const Json::Data::Handle &fail )
                : tested(     fail["Tested"]->As<size_t>() )
                , entropy(    fail["Entropy"]->As<size_t>() )
                , chisq(      fail["Chisq"]->As<size_t>() )
                , mean(       fail["Mean"]->As<size_t>() )
                , pi(         fail["Pi"]->As<size_t>() )
                , corr(       fail["Autocorr"]->As<size_t>() )
                , minentropy( fail["MinEntropy"]->As<size_t>() )
            {}


            std::string Report() const
            { //{{{

                return stringprintf( "Tested %zu, Hs %zu, Hm %zu, Mean %zu,"
                                     " Corr %zu, π %zu, χ² %zu",
                                     tested, entropy, minentropy, mean,
                                     corr, pi, chisq );
            } //}}}

            std::string AsJSON() const
            { //{{{

                return stringprintf( "{"
                                      "\"Tested\":%zu"
                                     ",\"Entropy\":%zu"
                                     ",\"Chisq\":%zu"
                                     ",\"Mean\":%zu"
                                     ",\"Pi\":%zu"
                                     ",\"Autocorr\":%zu"
                                     ",\"MinEntropy\":%zu"
                                     "}",
                                     tested,
                                     entropy, chisq, mean, pi, corr, minentropy );
            } //}}}

        }; //}}}

        static struct Results_Only_ {} Results_Only;

        struct Data
        { //{{{
        private:

            // Helper struct to rank and sort bins for reporting
            struct Bin
            { //{{{

                typedef std::vector< Bin >      Vector;

                unsigned    rank;
                unsigned    symbol;
                size_t      freq;


                Bin( unsigned s, size_t f )
                    : rank( 0 )
                    , symbol( s )
                    , freq( f )
                {}


                static bool ByFrequency( const Bin &a, const Bin &b )
                {
                    return a.freq > b.freq;
                }

                static std::string PrettyPrint( const Vector   &bins,
                                                size_t          nsamples,
                                                size_t          first_n = NBINS,
                                                size_t          last_n  = NBINS )
                { //{{{

                    double          dsamples = double(nsamples);
                    double          expected = dsamples / NBINS;
                    double          chisq    = 0;
                    size_t          min      = size_t(-1);
                    size_t          max      = 0;
                    std::string     s        = stringprintf( "Samples: %zu\n", nsamples );

                    if( first_n > 0 || last_n > 0 )
                    {
                        s += sizeof(T) > 1 ? " Rank                  Bin       Freq"
                                           : "Rank         Bin     Freq";

                        s += "               Error      χ²    % of Samples\n";
                    }

                    last_n = last_n < bins.size() ? bins.size() - last_n : 0;

                    for( size_t i = 0, e = bins.size(); i < e; ++i )
                    {
                        const Bin  &b   = bins[i];
                        double  dfreq   = double(b.freq);
                        double  error   = dfreq - expected;
                        double  errorsq = error * error / expected;

                        if( min > b.freq )
                            min = b.freq;

                        if( max < b.freq )
                            max = b.freq;

                        chisq += errorsq;

                        if( i >= first_n && i < last_n )
                            continue;

                        s += stringprintf( "%*u:  %s %.*x -> %-12zu  %+10.2f  %8.2f  %.9f\n",
                                           sizeof(T) > 1 ? 5 : 3, b.rank,
                                           AsBinary<T>(T(b.symbol)).c_str(),
                                           int(sizeof(T) * 2), b.symbol,
                                           b.freq, error, errorsq, dfreq / dsamples );
                    }

                    double  dmin = double(min);
                    double  dmax = double(max);

                    s += stringprintf( "  Expected %.3f,  %+.3f (%+.3f%%), %+.3f (%+.3f%%)\n"
                                       "  χ² %.2f (p = %f)",
                                       expected,
                                       dmax - expected, 100.0 * (dmax - expected) / expected,
                                       dmin - expected, 100.0 * (dmin - expected) / expected,
                                       chisq, pochisq(chisq, NBINS - 1) );
                    return s;

                } //}}}

            }; //}}}


        public:

            // Accumulators
            size_t          bin[ NBINS ];
            size_t          samples;

            size_t          inradius;
            size_t          pisamples;

            unsigned        corr0;
            unsigned        corrn;
            double          corr1;
            double          corr2;
            double          corr3;

            // Result cache
            Result          result[ DATASET_MAX ];
            Fail            fail;

            // Enable extra code to report debug information when the long term
            // normalisation occurs.  Note that you could be waiting for a very
            // long time for that to happen if size_t is a 64-bit type ...
            //#define CHECK_NORMALISATION

           #ifdef CHECK_NORMALISATION
            double          skew;
           #endif


            Data()
            {
                clear();
                result[MIN].clear( MIN );
                result[MAX].clear( MAX );
            }

            Data( const Json::Data::Handle &data )
                : samples( data["Samples"]->As<size_t>() )
                , inradius( data["PiIn"]->As<size_t>() )
                , pisamples( data["PiSamples"]->As<size_t>() )
            { //{{{

                Json::Data::Handle  binarray = data["Bins"];

                if( binarray->GetArraySize() != NBINS )
                    throw Error( _("Ent%zu::Data: invalid json with %zu bins"),
                                                NBITS, binarray->GetArraySize() );

                for( size_t i = 0; i < NBINS; ++i )
                    bin[i] = binarray[i]->As<size_t>();

                for( size_t i = 0; i < DATASET_MAX; ++i )
                    result[i] = Result( data[ DataSetName(DataSet(i)) ] );

                fail = Fail( data["Failed"] );

               #ifdef CHECK_NORMALISATION
                skew = 0.0;
               #endif

            } //}}}

            Data( Results_Only_, const Json::Data::Handle &data )
            { //{{{

                clear();
                samples = data["Samples"]->As<size_t>();

                for( size_t i = 0; i < DATASET_MAX; ++i )
                    result[i] = Result( data[ DataSetName(DataSet(i)) ] );

                fail = Fail( data["Failed"] );

            } //}}}


            void clear()
            { //{{{

                // We don't clear the result cache here, just the accumulators
                // so as to be ready to begin calculating the next set.  The
                // result cache is overwritten at the completion of that, but
                // kept around to be queried between runs.

                memset( bin, 0, sizeof(bin) );
                samples    = 0;
                inradius   = 0;
                pisamples  = 0;
                corr0      = NBINS + 1;
                corrn      = 0;
                corr1      = 0.0;
                corr2      = 0.0;
                corr3      = 0.0;

               #ifdef CHECK_NORMALISATION
                skew       = 0.0;
               #endif

            } //}}}

            void normalise_long_term()
            { //{{{

                if( samples > size_t(-1) / 2 )
                {
                   #ifdef CHECK_NORMALISATION
                    Log<0>( "Before:\n%s\n", ReportBinsByFreq(5,5).c_str() );
                    Log<0>( "s: %zu, ir: %zu, ps: %zu, c1: %f, c2: %f, c3: %f\n",
                            samples, inradius, pisamples, corr1, corr2, corr3 );
                   #endif


                    double  old_samples  = double(samples);
                    double  old_expected = old_samples / NBINS;
                    double  new_expected = old_expected / 2.0;

                    samples = 0;

                    for( size_t i = 0; i < NBINS; ++i )
                    {
                        double error = double(bin[i]) - old_expected;
                        double chisq = (error * error) / old_expected;
                        double fudge = sqrt(new_expected * chisq);

                        if( error < 0 )
                            bin[i] = size_t(lrint(new_expected - fudge));
                        else
                            bin[i] = size_t(lrint(new_expected + fudge));

                        samples += bin[i];
                    }

                    double  scale = double(samples) / old_samples;

                    corr1 *= scale;
                    corr2 *= scale;
                    corr3 *= scale;


                   #ifdef CHECK_NORMALISATION
                    Log<0>( "After:\n%s\n", ReportBinsByFreq(5,5).c_str() );
                    Log<0>( "s: %zu, ir: %zu, ps: %zu, c1: %f, c2: %f, c3: %f\n",
                            samples, inradius, pisamples, corr1, corr2, corr3 );

                    skew += samples - (old_samples / 2.0);

                    Log<0>( "Count skew: %0.1f (%0.15f), total %0.1f\n",
                            samples - (old_samples / 2.0), scale, skew );

                    Log<0>( "Before: %s\n", result[CURRENT].Report().c_str() );
                    ComputeResult();
                    Log<0>( "After : %s\n", result[CURRENT].Report().c_str() );
                   #endif
                }

                if( pisamples > size_t(-1) / 2 )
                {
                   #ifdef CHECK_NORMALISATION
                    Log<0>( "s: %zu, ir: %zu, ps: %zu\n", samples, inradius, pisamples );
                   #endif


                    inradius    >>= 1;
                    pisamples   >>= 1;


                   #ifdef CHECK_NORMALISATION
                    Log<0>( "s: %zu, ir: %zu, ps: %zu\n", samples, inradius, pisamples );

                    Log<0>( "Before: %s\n", result[CURRENT].Report().c_str() );
                    ComputeResult();
                    Log<0>( "After : %s\n", result[CURRENT].Report().c_str() );
                   #endif
                }

            } //}}}

            void AddResult( double entropy, double chisq, double mean,
                            double pi, double corr, double minentropy )
            { //{{{

                static const double     Mean = (1u << (NBITS - 1)) - 0.5;

                result[CURRENT].entropy     = entropy;
                result[CURRENT].chisq       = chisq;
                result[CURRENT].mean        = mean;
                result[CURRENT].pi          = pi;
                result[CURRENT].corr        = corr;
                result[CURRENT].minentropy  = minentropy;


                if( result[MIN].entropy > entropy )
                    result[MIN].entropy = entropy;

                if( result[MIN].chisq > chisq )
                    result[MIN].chisq = chisq;

                if( fabs(result[MIN].mean - Mean) > fabs(mean - Mean) )
                    result[MIN].mean = mean;

                if( fabs(result[MIN].pi - M_PI) > fabs(pi - M_PI) )
                    result[MIN].pi = pi;

                if( fabs(result[MIN].corr) > fabs(corr) )
                    result[MIN].corr = corr;

                if( result[MIN].minentropy > minentropy )
                    result[MIN].minentropy = minentropy;


                if( result[MAX].entropy < entropy )
                    result[MAX].entropy = entropy;

                if( result[MAX].chisq < chisq )
                    result[MAX].chisq = chisq;

                if( fabs(result[MAX].mean - Mean) < fabs(mean - Mean) )
                    result[MAX].mean = mean;

                if( fabs(result[MAX].pi - M_PI) < fabs(pi - M_PI) )
                    result[MAX].pi = pi;

                if( fabs(result[MAX].corr) < fabs(corr) )
                    result[MAX].corr = corr;

                if( result[MAX].minentropy < minentropy )
                    result[MAX].minentropy = minentropy;

            } //}}}

            void ComputeResult()
            { //{{{

                double dsamples = double(samples);
                double expected = dsamples / NBINS;
                double entropy  = 0.0;
                double chisq    = 0.0;
                double sum      = 0.0;

                size_t cmax     = 0;
                double pmax     = 0.0;

                for( size_t i = 0; i < NBINS; ++i )
                {
                    double  error = double(bin[i]) - expected;
                    double  p = double(bin[i]) / dsamples;

                    if( bin[i] > cmax )
                    {
                        cmax = bin[i];
                        pmax = p;
                    }

                    if( p > 0.0 )
                        entropy -= p * log2( p );

                    chisq   += (error * error) / expected;
                    sum     += double(bin[i] * i);
                }

                // The autocorrelation coefficient for N samples at lag h is:
                //   Rh = Ch / C0
                //
                // Where Ch is the autocovariance function:
                //   Ch = 1/N * Sum( t=1 -> N-h, (Yt - Ybar)(Ytplush - Ybar) )
                //
                // And C0 is the variance function:
                //   C0 = 1/N * Sum( t=1 -> N, (Yt - Ybar)^2 )
                //
                // where Ybar is the sample mean and Ytplush is the sample that
                // is h samples after Yt (because ascii).  Here we only compute
                // it for h=1.
                //
                // Rh ranges between -1.0 and 1.0, with 0 finding no correlation.

                double  c1 = corr1 + corrn * corr0;
                double  c2 = corr2 * corr2;
                double  ac = (dsamples * c1 - c2) / (dsamples * corr3 - c2);

                AddResult( entropy, chisq, sum / dsamples,
                           4.0 * double(inradius) / double(pisamples),
                           // report 1.0 if samples*c3-c2 == 0
                           std::isfinite(ac) ? ac : 1.0,
                           // NIST SP 800-90B Section 9.2, min-entropy of IID sources
                           -log2( (double(cmax) + 2.3 * sqrt(dsamples * pmax * (1.0 - pmax)))
                                  / dsamples ) );
            } //}}}


            std::string ReportResult( DataSet set = CURRENT ) const
            {
                return stringprintf( "%zu: ", samples ) + result[set].Report();
            }

            std::string ReportResults() const
            { //{{{

                std::string     s = stringprintf( "Samples: %zu", samples );

                for( unsigned i = 0; i < DATASET_MAX; ++i )
                    s += stringprintf( "\n%7s: ", DataSetName(DataSet(i)) )
                       + result[i].Report();

                s += "\nFailure: " + fail.Report();

                return s;

            } //}}}

            // Present the bins ordered by symbol value
            std::string ReportBins( size_t first_n = NBINS, size_t last_n = NBINS ) const
            { //{{{

                typename Bin::Vector    bins;

                for( size_t i = 0; i < NBINS; ++i )
                    bins.push_back( Bin( unsigned(i), bin[i] ) );

                typename Bin::Vector    sorted_bins( bins );

                std::stable_sort( sorted_bins.begin(), sorted_bins.end(), Bin::ByFrequency );

                for( size_t i = 0; i < NBINS; ++i )
                    bins[ sorted_bins[i].symbol ].rank = unsigned(i + 1);

                return Bin::PrettyPrint( bins, samples, first_n, last_n );

            } //}}}

            // Present the bins ordered by symbol frequency
            std::string ReportBinsByFreq( size_t first_n = NBINS, size_t last_n = NBINS ) const
            { //{{{

                typename Bin::Vector    bins;

                for( size_t i = 0; i < NBINS; ++i )
                    bins.push_back( Bin( unsigned(i), bin[i] ) );

                std::stable_sort( bins.begin(), bins.end(), Bin::ByFrequency );

                for( size_t i = 0; i < NBINS; ++i )
                    bins[i].rank = unsigned(i + 1);

                return Bin::PrettyPrint( bins, samples, first_n, last_n );

            } //}}}


            std::string ResultAsJSON( DataSet set ) const
            {
                return '"' + std::string(DataSetName(set)) + "\":" + result[set].AsJSON();
            }

            std::string ResultsAsJSON() const
            { //{{{

                return stringprintf( "{" "\"Samples\":%zu", samples )
                                   + ',' + ResultAsJSON( CURRENT )
                                   + ',' + ResultAsJSON( MIN )
                                   + ',' + ResultAsJSON( MAX )
                                   + ",\"Failed\":" + fail.AsJSON()
                                   + '}';
            } //}}}

            std::string AsJSON() const
            { //{{{

                std::string     s;

                s = stringprintf( "{"
                                   "\"Samples\":%zu"
                                  ",\"Bins\":[%zu", samples, bin[0] );

                for( size_t i = 1; i < NBINS; ++i )
                    s += stringprintf( ",%zu", bin[i] );

                s += stringprintf( "]"
                                   ",\"PiSamples\":%zu"
                                   ",\"PiIn\":%zu",
                                   pisamples, inradius );

                for( unsigned d = 0; d < DATASET_MAX; ++d )
                    s += ',' + ResultAsJSON( DataSet(d) );

                s += ",\"Failed\":" + fail.AsJSON();

                return s + '}';

            } //}}}

        }; //}}}


    private:

        // Bytes used as Monte Carlo co-ordinates. This should be no more
        // bits than the mantissa of the "double" floating point type.
        static const unsigned MONTE_BYTES = 6;

        size_t      m_short_len;
        uint64_t    m_radius;

        Data        m_short;
        Data        m_previous_short;
        Data        m_long;
        bool        m_have_results;

        bool        m_have_unchecked_results;
        size_t      m_entropy_converged;
        size_t      m_mean_converged;
        size_t      m_pi_converged;
        size_t      m_corr_converged;
        size_t      m_minentropy_converged;
        size_t      m_ok_wait;



        // This one always operates on 8 bit samples, even in 16-bit mode
        void analyse_monte( const uint8_t *buf, size_t len )
        { //{{{

            // Are we inside or outside the radius of a circle with 24bit coordinates
            for( size_t i = 0; i + MONTE_BYTES < len; i += MONTE_BYTES )
            {
                uint64_t    x = 0, y = 0;

                for( size_t j = 0; j < MONTE_BYTES / 2; ++j )
                {
                    x = x * 256 + buf[i+j];
                    y = y * 256 + buf[MONTE_BYTES/2+i+j];
                }

                if( x*x + y*y <= m_radius )
                    ++m_short.inradius;

                ++m_short.pisamples;
            }

        } //}}}

        void analyse( const T *buf, size_t len )
        { //{{{

            for( size_t i = 0; i < len; ++i )
            {
                // Count bin frequencies for entropy and Chi^2 calculation
                m_short.bin[ buf[i] ]++;

                // Compute autocorrelation
                if( m_short.corr0 > NBINS )
                    m_short.corr0 = buf[i];
                else
                    m_short.corr1 += m_short.corrn * buf[i];

                m_short.corrn  = buf[i];
                m_short.corr2 += m_short.corrn;
                m_short.corr3 += m_short.corrn * m_short.corrn;
            }

            m_short.samples += len;


            if( m_short.samples == m_short_len )
                flush();

        } //}}}

        const Limits &GetLimits() const;


    public:

        Ent( size_t short_len = 0 )
            : m_short_len( short_len ? short_len
                                     : NBITS == 8 ? 500000
                                                  : 100000000 )
            , m_radius( uint64_t(floor( pow( pow(256.0, MONTE_BYTES / 2) - 1.0, 2.0 ) )) )
            , m_have_results( false )
            , m_have_unchecked_results( false )
            , m_entropy_converged( 0 )
            , m_mean_converged( 0 )
            , m_pi_converged( 0 )
            , m_corr_converged( 0 )
            , m_minentropy_converged( 0 )
            , m_ok_wait( 1 )
        {
            Log<2>( "+ Ent%zu( %zu )\n", NBITS, m_short_len );
        }

        ~Ent()
        {
            Log<2>( "- Ent%zu( %zu )\n", NBITS, m_short_len );
        }


        void clear()
        {
            m_short.clear();
            m_long.clear();
        }

        // You don't usually want to call this, except to flush a final block
        // of samples when the input may not be a multiple of the short block
        // length, and when you really only care about the results of the long
        // term analysis (since the short block result will not be typical if
        // it is analysed at a different length to the previous blocks).
        void flush()
        { //{{{

            if( m_short.samples == 0 )
                return;

            size_t  long_minsamples = GetLimits().long_minsamples;
            size_t  long_samples    = m_long.samples;

            for( size_t i = 0; i < NBINS; ++i )
                m_long.bin[i] += m_short.bin[i];

            if( m_long.corr0 > NBINS )
                m_long.corr0 = m_short.corr0;

            m_long.corrn      = m_short.corrn;
            m_long.corr1     += m_short.corr1;
            m_long.corr2     += m_short.corr2;
            m_long.corr3     += m_short.corr3;

            m_long.inradius  += m_short.inradius;
            m_long.pisamples += m_short.pisamples;
            m_long.samples   += m_short.samples;

            m_short.ComputeResult();
            m_long.ComputeResult();

            m_long.normalise_long_term();
            m_previous_short = m_short;
            m_short.clear();

            m_have_results              = true;
            m_have_unchecked_results    = true;

            // Reset the long term min/max watermarks once we have enough samples
            // collected to expect they should remain within the tighter failure
            // limits if everything is working correctly.  They aren't as useful
            // to us if they always get set during the first few sample periods.
            if( long_samples <= long_minsamples && m_long.samples > long_minsamples )
                m_long.result[MIN] = m_long.result[MAX] = m_long.result[CURRENT];

        } //}}}


        void Analyse( const uint8_t *buf, size_t len )
        { //{{{

            size_t  sample_len = len / sizeof(T);

            if( m_short.samples + sample_len > m_short_len )
            {
                size_t  r = (sample_len - (m_short_len - m_short.samples)) * sizeof(T);
                Analyse( buf, r );
                Analyse( buf + r, len - r );
                return;
            }

            // We probably can't always be guaranteed of alignment here, so we
            // might need to copy this on arches that care about that a lot.
            // But all the currently existing users of it that we have, do have
            // sufficient alignment to cast to (at least) uint16_t, so suppress
            // the compile time warning and throw at runtime if that's not true.
            //
            // If this ever isn't true, we can use IsAligned to check if we need
            // to copy it to an aligned bounce buffer first.
            try {
                const T *b = aligned_recast< const T* >( buf );

                analyse_monte( buf, len );
                analyse( b, sample_len );
            }
            catch( const std::exception &e )
            {
                throw Error( "Ent%zu::Analyse: %s", NBITS, e.what() );
            }

        } //}}}

        bool IsOk( bool was_ok = true )
        { //{{{

            if( ! m_have_results )
                return false;

            if( ! m_have_unchecked_results )
                return m_ok_wait == 1;

            m_have_unchecked_results = false;

            m_short.fail.tested++;
            m_long.fail.tested++;


            const Limits   &lim     = GetLimits();
            const Result   &sr      = m_short.result[CURRENT];
            const Result   &lr      = m_long.result[CURRENT];
            bool            passed  = true;

            if( sr.entropy < lim.short_entropy )
            {
                m_short.fail.entropy++;
                passed = false;
            }

            if( lr.entropy < lim.long_entropy )
            {
                if( m_long.samples > lim.long_minsamples )
                {
                    m_long.fail.entropy++;
                    m_entropy_converged = 0;
                    passed = false;
                }
            }
            else if( ! m_entropy_converged )
            {
                m_entropy_converged = m_long.samples;
            }


            if( sr.minentropy < lim.short_minentropy )
            {
                m_short.fail.minentropy++;
                passed = false;
            }

            if( lr.minentropy < lim.long_minentropy )
            {
                if( m_long.samples > lim.long_minsamples )
                {
                    m_long.fail.minentropy++;
                    m_minentropy_converged = 0;
                    passed = false;
                }
            }
            else if( ! m_minentropy_converged )
            {
                m_minentropy_converged = m_long.samples;
            }


            if( sr.chisq < lim.short_chisq_min || sr.chisq > lim.short_chisq_max )
            {
                m_short.fail.chisq++;
                passed = false;
            }

            if( lr.chisq < lim.long_chisq_min || lr.chisq > lim.long_chisq_max )
            {
                m_long.fail.chisq++;
                passed = false;
            }


            if( sr.mean < lim.short_mean_min || sr.mean > lim.short_mean_max )
            {
                m_short.fail.mean++;
                passed = false;
            }

            if( lr.mean < lim.long_mean_min || lr.mean > lim.long_mean_max )
            {
                if( m_long.samples > lim.long_minsamples )
                {
                    m_long.fail.mean++;
                    m_mean_converged = 0;
                    passed = false;
                }
            }
            else if( ! m_mean_converged )
            {
                m_mean_converged = m_long.samples;
            }


            if( sr.pi < M_PI - lim.short_pi || sr.pi > M_PI + lim.short_pi )
            {
                m_short.fail.pi++;
                passed = false;
            }

            if( lr.pi < M_PI - lim.long_pi || lr.pi > M_PI + lim.long_pi )
            {
                if( m_long.samples > lim.long_minsamples )
                {
                    m_long.fail.pi++;
                    m_pi_converged = 0;
                    passed = false;
                }
            }
            else if( ! m_pi_converged )
            {
                m_pi_converged = m_long.samples;
            }


            if( sr.corr < -lim.short_corr || sr.corr > lim.short_corr )
            {
                m_short.fail.corr++;
                passed = false;
            }

            if( lr.corr < -lim.long_corr || lr.corr > lim.long_corr )
            {
                if( m_long.samples > lim.long_minsamples )
                {
                    m_long.fail.corr++;
                    m_corr_converged = 0;
                    passed = false;
                }
            }
            else if( ! m_corr_converged )
            {
                m_corr_converged = m_long.samples;
            }


            // If we're told the current state is "not ok", and we've seen
            // a measured failure within the last recovery_blocks short blocks,
            // then we report things are still not ok until we're sure that the
            // recovery is actually sustained.  If we haven't actually seen any
            // failure, then report the current status immediately.
            //
            // This allows two things:
            //
            // - An assumption at startup that things are not ok until we've
            //   actually seen that the first block probably is.  In that case
            //   we don't want to wait a long time, since the system might be
            //   waiting on us to provide sufficent boot entropy.
            //
            // - A mechanism to bypass the recovery hysteresis for analysis
            //   code that really wants to know the true state of each block
            //   individually (or that wants to provide its own rules for
            //   deciding when a recovery is believed to have happened).

            if( passed )
            {
                if( ! was_ok && m_ok_wait != 1
                 && m_long.samples - m_ok_wait < lim.recovery_blocks * m_short_len )
                {
                    passed = false;

                } else {

                    // m_long.samples increments in multiples of m_short_len, so
                    // this value should never be seen if the recovery timer is
                    // actually running.
                    m_ok_wait = 1;
                }

            } else {

                m_ok_wait = m_long.samples;
            }

            m_previous_short.fail = m_short.fail;

            return passed;

        } //}}}


        bool HaveResults() const            { return m_have_results; }


        const Result &ShortTermResult( DataSet set = CURRENT ) const
        {
            return m_short.result[set];
        }

        const Result &LongTermResult( DataSet set = CURRENT ) const
        {
            return m_long.result[set];
        }


        const Data &ShortTermData() const   { return m_previous_short; }

        const Data &LongTermData() const    { return m_long; }


        std::string ResultsAsJSON() const
        { //{{{

            return stringprintf( "\"Ent%zu\":{", NBITS )
                                          + "\"Short\":" + m_previous_short.ResultsAsJSON()
                                          + ",\"Long\":" + m_long.ResultsAsJSON()
                                          + '}';
        } //}}}

        std::string AsJSON() const
        { //{{{

            return stringprintf( "\"Ent%zu\":{", NBITS )
                                          + "\"Short\":" + m_previous_short.AsJSON()
                                          + ",\"Long\":" + m_long.AsJSON()
                                          + '}';
        } //}}}

    }; //}}}

    template< typename T>
    typename Ent<T>::Results_Only_ Ent<T>::Results_Only;

    // Don't warn about using "GNU old-style field designators" with clang,
    // or emit a warning if -Wgnu-designator is an unknown option elsewhere.
    // When C++ gives us something we can use which is nicer than this for
    // initialising POD structures, we will use it.  Until then this is it.
    EM_TRY_PUSH_DIAGNOSTIC_IGNORE("-Wgnu-designator")

    template <> inline const Ent<uint8_t>::Limits &Ent<uint8_t>::GetLimits() const
    { //{{{

        // We allow the long term measures 250MB to converge on the ranges
        // that we test for here.  Most of those will converge to limits
        // which are much tighter than we allow for here, but that can take
        // quite a bit longer before they are sure to be stable within it,
        // even when all is working just as it should inside the expected
        // degree of variability.  A generator running at 1Mbps will take
        // a bit over 30 minutes to reach this threshold, so this is a
        // reasonable compromise between tight bounds and the amount of
        // time needed before we can test them.  Any failure that would
        // trip these bounds before that time will either also trip the
        // short term limits (or other tests like the FIPS set), or will
        // be not so far from statistically random that it couldn't be a
        // legitimate sequence from the long end of the tail.
        //
        // Some of them are likely to converge faster than this, at least
        // in the best or average case, but it doesn't seem to be worth
        // having the extra complication of individual lengths for them,
        // we can just use a tighter bound for those rather than trying
        // to optimise limits for them in multiple dimensions.

        static const Limits     lim8 =
        {
            long_minsamples:    250000000,

            long_entropy:       7.999999,
            short_entropy:      7.999,

            // Random expected outside than this less than once in 1 million trials
            long_chisq_min:     161.643,
            long_chisq_max:     377.053,

            // Random expected outside than this less than once in 100 million trials
            short_chisq_min:    147.374,
            short_chisq_max:    400.965,

            long_mean_min:      127.5 - 0.019,
            long_mean_max:      127.5 + 0.019,
            short_mean_min:     127.5 - 0.58,
            short_mean_max:     127.5 + 0.58,

            long_pi:            0.0003 * M_PI,
            short_pi:           0.0097 * M_PI,

            long_corr:          0.00025,
            short_corr:         0.0078,

            long_minentropy:    7.99,
            short_minentropy:   7.73,

            recovery_blocks:    10,
        };

        return lim8;

    } //}}}

    template <> inline const Ent<uint16_t>::Limits &Ent<uint16_t>::GetLimits() const
    { //{{{

        // We allow the long term measures 800MB to converge on the ranges that
        // we test for here.  Most of those will converge to limits which are
        // much tighter than we allow for here, but that can take quite a bit
        // longer before they are sure to be stable within it, even when all is
        // working just as it should inside the expected degree of variability.
        // A generator running at 1Mbps will take a bit under two hours to reach
        // this threshold, which is a bit less reasonable than the compromise we
        // make for 8 bit data, but we do also need a lot more data here to get
        // similarly significant results with 16-bit words.  Ideally it should
        // probably be even larger than this, so that we can have bounds which
        // are about as tight as the 8-bit ones are (or have dynamic bounds that
        // tighten up as we do get more data), but this offers us a reasonable
        // sanity check for a first pass that will trigger in a not totally
        // unreasonable time if it's actually the first test to detect some
        // failure (which it is reasonably unlikely to be).  The 16-bit analysis
        // is more for reassuring people who want to look at the data in detail
        // than for automatic health checks, so the delay is more 'annoying to
        // people' than a risk to the integrity of the system.
        //
        // In a similar vein we only require 3 good blocks here before lifting
        // the failure lockout, which isn't as big a compromise on assurance of
        // correct functioning as it might first seem.  With the default short
        // block lengths of 500k samples for 8-bit testing and 100M samples for
        // the 16-bit tests, that means at least 600 8-bit tests need to have
        // continued passing during that time (as well as all the other tests)
        // if normal functioning is actually to resume.  If we expect the same
        // number of good blocks here as we do for the 8-bit testing, then a
        // single transient failure can lock the device out for several hours,
        // and we can't totally rule out an occasionally exceptional result,
        // Because Random.  There is a balancing game between making the limits
        // here tight enough to quickly catch abnormality but not so tight that
        // they are expected to be exceeded with 'annoying regularity' by a
        // properly functioning device.

        static const Limits     lim16 =
        {
            long_minsamples:    800000000,

            long_entropy:       15.9999,
            short_entropy:      15.9995,

            // Random expected outside than this less than once in 1 million trials
            long_chisq_min:     63823.624,
            long_chisq_max:     67265.364,

            // Random expected outside than this less than once in 100 million trials
            short_chisq_min:    321.0,
            short_chisq_max:    67459.181,

            long_mean_min:      32767.5 - 1.87,
            long_mean_max:      32767.5 + 1.87,
            short_mean_min:     32767.5 - 7.69,
            short_mean_max:     32767.5 + 7.69,

            long_pi:            0.000088 * M_PI,
            short_pi:           0.000395 * M_PI,

            long_corr:          0.00011,
            short_corr:         0.00044,

            long_minentropy:    15.893,
            short_minentropy:   15.708,

            recovery_blocks:    3,
        };

        return lim16;

    } //}}}

    EM_POP_DIAGNOSTIC

    // Convenience types for 8 and 16 bit Ent instances
    typedef Ent<uint8_t>    Ent8;
    typedef Ent<uint16_t>   Ent16;



    // Tracks the length of runs of consecutive 0 or 1 bits, and the number of
    // runs of each length.  This turns out to also be the "General Runs Test"
    // described in AIS-31, though we were doing this before I read that.  It
    // can be run as a standalone test on an arbitrary length block of data,
    // but we also populate it from the FIPS test analysis routine (since we
    // are already counting runs of consecutive bits there so populating this
    // structure with that information is essentially 'free').
    template< size_t MaxRun >
    class BitRun
    { //{{{
    public:

        struct Result
        { //{{{
        private:

            mutable double      m_expected[ MaxRun ];
            mutable double      m_err[2][ MaxRun ];
            mutable double      m_chisq;
            mutable double      m_chisqp;
            mutable unsigned    m_chisqk;


            void compute_chisq() const
            { //{{{

                if( m_chisqk )
                    return;

                double  nbits = double(total[0] + total[1]);
                double  div   = 8.0;                // 2^3

                m_chisq = 0.0;

                for( size_t i = 0, e = std::min(maxrun, MaxRun); i < e; ++i )
                {
                    // The expected number of runs of length i in a
                    // sequence of n bits is (n - i + 3) / 2^(i+2)
                    m_expected[i]   = (nbits - double(i) + 2.0) / div;
                    m_err[0][i]     = double(runlengths[0][i]) - m_expected[i];
                    m_err[1][i]     = double(runlengths[1][i]) - m_expected[i];

                    if( m_expected[i] >= 5.0 )
                    {
                        m_chisq += m_err[0][i] * m_err[0][i] / m_expected[i]
                                 + m_err[1][i] * m_err[1][i] / m_expected[i];
                        m_chisqk = unsigned(i);
                    }

                    div *= 2.0;
                }

                for( size_t i = maxrun; i < MaxRun; ++i )
                {
                    m_expected[i] = (nbits - double(i) + 2) / div;

                    if( m_expected[i] >= 5.0 )
                    {
                        m_err[0][i] = double(runlengths[0][i]) - m_expected[i];
                        m_err[1][i] = double(runlengths[1][i]) - m_expected[i];

                        m_chisq += m_err[0][i] * m_err[0][i] / m_expected[i]
                                 + m_err[1][i] * m_err[1][i] / m_expected[i];
                        m_chisqk = unsigned(i);
                    }
                    else
                        break;

                    div *= 2.0;
                }

                // The χ² degrees of freedom are k = 2 * i - 1, where i is the
                // longest runlength expected to occur at least 5 times.  We
                // could approximate this from GetExpectedMax() - 4, but we'd
                // still need a test inside the loop for whether to add the
                // error term to the m_chisq total, so that doesn't gain us a
                // lot in this function.
                //
                // The probability computed here is still a bit handwavy in any
                // case.  The distribution of runs is only approximated by χ²,
                // and the cutoff of "at least 5" is an arbitrary convention,
                // that has a long history of use but no real formal proof of
                // its significance compared to other reasonable but arbitrary
                // cutoff values.  It gives us a reaonable guide for whether
                // some result is "about right" or "way off what is expected",
                // but the boundary between those two is less well defined in
                // this case.

                m_chisqk = m_chisqk * 2 + 1;    // m_chisqk == i-1 before this
                m_chisqp = pochisq( m_chisq, m_chisqk );

            } //}}}


        public:

            size_t      runlengths[2][ MaxRun ];
            size_t      total[2];
            size_t      maxrun;


            Result()
                : m_chisq( 0.0 )
                , m_chisqp( 0.0 )
                , m_chisqk( 0 )
                , maxrun( 0 )
            {
                memset( m_expected, 0, sizeof(m_expected) );
                memset( m_err,      0, sizeof(m_err) );
                memset( runlengths, 0, sizeof(runlengths) );
                memset( total,      0, sizeof(total) );
            }

            Result( const Json::Data::Handle &result )
                : m_chisq(  result["Chisq"] )
                , m_chisqp( result["Chisq-p"] )
                , m_chisqk( result["Chisq-k"]->As<unsigned>() )
                , maxrun(   result["Max"]->As<size_t>() )
            { //{{{

                Json::Data::Handle  runs  = result["Runs"];
                size_t              nruns = runs->GetArraySize();

                if( nruns > MaxRun )
                    throw Error( _("BitRun<%zu>::Result: invalid json with %zu runs"),
                                                                        MaxRun, nruns );

                memset( m_expected, 0, sizeof(m_expected) );
                memset( m_err,      0, sizeof(m_err) );
                memset( runlengths, 0, sizeof(runlengths) );

                total[0] = result["Zeros"]->As<size_t>();
                total[1] = result["Ones"]->As<size_t>();

                for( size_t i = 0; i < nruns; ++i )
                {
                    runlengths[0][i] = runs[i][0]->As<size_t>();
                    runlengths[1][i] = runs[i][1]->As<size_t>();
                    m_expected[i]    = runs[i][2];
                    m_err[0][i]      = double(runlengths[0][i]) - m_expected[i];
                    m_err[1][i]      = double(runlengths[1][i]) - m_expected[i];
                }

            } //}}}


            void InvalidateChisq()  { m_chisqk = 0; }


            double GetBias() const  { return double(total[0]) / double(total[1]); }

            double GetChisq( double *p = NULL ) const
            { //{{{

                compute_chisq();

                if( p )
                    *p = m_chisqp;

                return m_chisq;

            } //}}}

            // Returns the runlength that is expected with probability ~0.5.
            // The actual maximum runlength which is seen has a fairly high
            // probability of being within about +/- 3 of this value.
            size_t GetExpectedMax() const
            {
                return size_t(lrint( log2( double(total[0] + total[1]) / 2.0 ) ));
            }


            // Tell UBSan that it's ok to have Inf and NaN results here as the
            // 'correctly reported' limits of some calculations, as defined by
            // IEC 559 / IEEE 754 for floating point math.
            BB_NO_SANITIZE_FLOAT_DIVIDE_BY_ZERO
            std::string Report() const
            { //{{{

                compute_chisq();

                std::string     s( "run of        zeros         ones     bias"
                                   "               expect     e0 %       e1 %"
                                   "       e0²/e      e1²/e    d0         d1\n" );

                for( size_t i = 0, e = std::min(maxrun, MaxRun); i < e; ++i )
                {
                    size_t zeros    = runlengths[0][i];
                    size_t ones     = runlengths[1][i];
                    double e0       = m_err[0][i];
                    double e1       = m_err[1][i];
                    double expected = m_expected[i];

                    s += stringprintf( "%5zu: %12zu %12zu %12.6f %16.3f "
                                       "% 10.4f % 10.4f %9.2f%c %9.2f%c",
                                       i + 1, zeros, ones, double(zeros)/double(ones), expected,
                                       100.0 * e0 / expected, 100.0 * e1 / expected,
                                       e0 * e0 / expected, double(zeros) < expected ? '-' : ' ',
                                       e1 * e1 / expected, double(ones)  < expected ? '-' : ' ' );
                    if( i )
                        s += stringprintf( " %10.6f %10.6f\n",
                                           double(zeros) / double(runlengths[0][i-1]),
                                            double(ones) / double(runlengths[1][i-1]) );
                    else
                        s += '\n';
                }

                s += stringprintf( "\ntotal: %12zu %12zu %12.6f            χ² = %f (p = %f), k = %u",
                                   total[0], total[1], double(total[0]) / double(total[1]),
                                   m_chisq, m_chisqp, m_chisqk );

                if( maxrun >= MaxRun )
                    s += stringprintf( "\nMax run was %zu", maxrun );

                return s;

            } //}}}

            std::string AsJSON() const
            { //{{{

                compute_chisq();

                std::string     s( 1, '{' );

                s += stringprintf( "\"Zeros\":%zu"
                                   ",\"Ones\":%zu"
                                   ",\"Max\":%zu", total[0], total[1], maxrun );

                s += ",\"Runs\":[";
                for( size_t i = 0, n = std::min(maxrun, MaxRun); i < n; ++i )
                {
                    if( i )
                        s += ',';

                    s += stringprintf( "[%zu,%zu,%f]", runlengths[0][i],
                                                       runlengths[1][i], m_expected[i] );
                }
                s += ']';

                s += stringprintf( ",\"Chisq\":%f"
                                   ",\"Chisq-p\":%f"
                                   ",\"Chisq-k\":%u", m_chisq, m_chisqp, m_chisqk );
                s += '}';

                return s;

            } //}}}

        }; //}}}


    private:

        Result      m_result;
        size_t      m_runlength;
        unsigned    m_runbit;


    public:

        BitRun()
            : m_runlength( 0 )
            , m_runbit( 2 )
        {}


        void clear()
        {
            m_result    = Result();
            m_runlength = 0;
            m_runbit    = 2;
        }

        // You don't usually want to call this, except to flush the final run of
        // bits at the ultimate completion of analysis.  If more bits are added
        // after it is called, the first bit will be considered as the start of
        // a new run, even if it is the same as the last bit previously added.
        void flush()
        { //{{{

            if( __builtin_expect(m_runbit != 2, 1) )
            {
                m_result.InvalidateChisq();

                if( m_result.maxrun < m_runlength )
                    m_result.maxrun = m_runlength;

                size_t n = m_runlength < MaxRun ? m_runlength - 1 : MaxRun - 1;

                ++m_result.runlengths[m_runbit][n];

                m_runlength = 0;
                m_runbit    = 2;
            }

        } //}}}


        void AddBits( unsigned bit, size_t len )
        { //{{{

            m_result.InvalidateChisq();
            m_result.total[bit] += len;

            if( __builtin_expect(m_runbit != 2, 1) )
            {
                if( bit == m_runbit )
                {
                    m_runlength += len;
                    return;
                }

                if( m_result.maxrun < m_runlength )
                    m_result.maxrun = m_runlength;

                size_t n = m_runlength < MaxRun ? m_runlength - 1 : MaxRun - 1;

                ++m_result.runlengths[m_runbit][n];
            }

            m_runlength = len;
            m_runbit    = bit;

        } //}}}

        void AddBits( const uint8_t *buf, size_t len )
        { //{{{

            unsigned run_length = 0;
            unsigned run_bit    = 2;

            for( size_t i = 0; i < len; ++i )
            {
                for( int b = 7; b >= 0; --b )
                {
                    unsigned    bit = buf[i] & 1u << b ? 1 : 0;
                    if( bit == run_bit )
                    {
                        ++run_length;
                        continue;
                    }

                    if( __builtin_expect(run_bit != 2, 1) )
                        AddBits( run_bit, run_length );

                    run_bit    = bit;
                    run_length = 1;
                }
            }

            if( __builtin_expect(run_bit != 2, 1) )
                AddBits( run_bit, run_length );

        } //}}}


        const Result &GetResult() const
        {
            return m_result;
        }

    }; //}}}

    // Convenience type for a default MaxRun length of 64.
    // The probability of runs up to this length in a uniformly random distribution
    // is about 1 in 37 Exabits, so this should be enough resolution for most uses.
    typedef BitRun<64>      BitRuns;



    // Rolling statistics on runs of consecutive test passes
    template< size_t Q, size_t PERIOD >
    class PassRuns
    { //{{{
    private:

        size_t      m_count;
        size_t      m_runs;
        size_t      m_previous;
        size_t      m_avg;
        size_t      m_long_avg;
        size_t      m_peak;


    public:

        static const size_t AVG_Q       = Q;
        static const size_t AVG_PERIOD  = PERIOD;


        PassRuns()
            : m_count( 0 )
            , m_runs( 0 )
            , m_previous( 0 )
            , m_avg( 0 )
            , m_long_avg( 0 )
            , m_peak( 0 )
        {}

        PassRuns( const Json::Data::Handle &pass )
            : m_count( 0 )
            , m_runs(     pass["Runs"]->As<size_t>() )
            , m_previous( pass["Previous"]->As<size_t>() )
            , m_avg(      pass["Short"]->As<size_t>() << AVG_Q )
            , m_long_avg( pass["Long"]->As<size_t>() * m_runs )
            , m_peak(     pass["Peak"]->As<size_t>() )
        {}


        void clear()
        { //{{{

            m_count     = 0;
            m_runs      = 0;
            m_previous  = 0;
            m_avg       = 0;
            m_long_avg  = 0;
            m_peak      = 0;

        } //}}}

        void Pass()
        {
            ++m_count;
        }

        // Returns true if the peak run length was exceeded
        bool Fail()
        { //{{{

            m_runs++;
            m_avg       = (m_avg * (AVG_PERIOD - 1) + (m_count << AVG_Q)) / AVG_PERIOD;
            m_long_avg += m_count;

            if( m_long_avg > size_t(-1) / 2 || m_runs > size_t(-1) / 2 )
            {
                m_long_avg >>= 1;
                m_runs >>= 1;
            }

            if( m_count > m_peak )
            {
                m_peak      = m_count;
                m_previous  = m_count;
                m_count     = 0;
                return true;
            }

            m_previous  = m_count;
            m_count     = 0;

            return false;

        } //}}}


        size_t Runs() const         { return m_runs; }

        size_t LongTerm() const     { return m_runs ? m_long_avg / m_runs : 0; }

        size_t ShortTerm() const    { return m_avg >> AVG_Q; }

        size_t Peak() const         { return m_peak; }

        size_t Current() const      { return m_count; }

        size_t Previous() const     { return m_previous; }


        std::string Report() const
        {
            return stringprintf( "%zu, short %zu, long %zu, peak %zu",
                                 m_runs, ShortTerm(), LongTerm(), m_peak );
        }

        std::string AsJSON() const
        { //{{{

            return stringprintf( "{" "\"Runs\":%zu"
                                     ",\"Previous\":%zu"
                                     ",\"Short\":%zu"
                                     ",\"Long\":%zu"
                                     ",\"Peak\":%zu"
                                 "}",
                                 m_runs, m_previous, ShortTerm(), LongTerm(), m_peak );
        } //}}}

    }; //}}}

    // Rolling statistics on test failure rates
    template< size_t Q, size_t PERIOD >
    struct FailRate
    { //{{{
    private:

        void normalise_long_term()
        { //{{{

            // We could keep a count of the number of times we have
            // needed to normalise the long term numbers to avoid
            // overflow, but it's probably not an interesting stat.
            if( pass + fail > size_t(-1) / 2 )
            {
                pass >>= 1;
                fail >>= 1;
            }

        } //}}}


    public:

        static const size_t AVG_Q       = Q;
        static const size_t AVG_PERIOD  = PERIOD;

        size_t      pass;
        size_t      fail;
        size_t      peak;
        size_t      rate;


        FailRate()
            : pass( 0 )
            , fail( 0 )
            , peak( 0 )
            , rate( 0 )
        {}

        FailRate( const Json::Data::Handle &stats )
            : pass( stats["Passed"]->As<size_t>() )
            , fail( stats["Failed"]->As<size_t>() )
            , peak( size_t(stats["Peak"] * (1u << AVG_Q)) )
            , rate( size_t(stats["Short"] * (1u << AVG_Q)) )
        {}


        void clear()
        { //{{{

            pass = 0;
            fail = 0;
            peak = 0;
            rate = 0;

        } //}}}

        void Pass()
        { //{{{

            ++pass;
            rate = rate * (AVG_PERIOD - 1) / AVG_PERIOD;
            normalise_long_term();

        } //}}}

        // Returns true if the peak (short term) failure rate was exceeded
        bool Fail()
        { //{{{

            ++fail;

            // We could probably just do this for each Pass, since any
            // test that fails every time has bigger problems than an
            // overflowing size_t on the number of times it did.
            normalise_long_term();

            rate = (rate * (AVG_PERIOD - 1) + (1u<<AVG_Q)) / AVG_PERIOD;

            if( rate > peak )
            {
                peak = rate;
                return true;
            }

            return false;

        } //}}}


        double LongTerm() const  { return pass + fail ? double(fail) / double(pass + fail)
                                                      : 0.0; }

        double ShortTerm() const { return double(rate) / (1u << AVG_Q); }

        double Peak() const      { return double(peak) / (1u << AVG_Q); }


        std::string Report() const
        {
            return stringprintf( "%zu / %zu, short %f, long %f, peak %f",
                                 fail, pass + fail, ShortTerm(), LongTerm(), Peak() );
        }

        std::string AsJSON() const
        { //{{{

            return stringprintf( "{" "\"Passed\":%zu"
                                     ",\"Failed\":%zu"
                                     ",\"Short\":%f"
                                     ",\"Long\":%f"
                                     ",\"Peak\":%f"
                                 "}",
                                 pass, fail, ShortTerm(), LongTerm(), Peak() );
        } //}}}

    }; //}}}


    // Implements the tests from FIPS 140-2, including the Continuous RNG Test
    // from section 4.9.2, and the statistical tests (which were removed from
    // section 4.9.1 in Change Notice 2). These aren't particularly good tests
    // to evaluate the quality of a RNG, or for detecting subtle manipulation
    // of its output, but they are fast to run, and should quickly catch most
    // genuine failure modes of a known-good generator or any unsophisticated
    // attempt to degrade its operation in some way during normal running.
    //
    // It also implements the Adaptive Proportion Test from the NIST SP 800-90B
    // 2012 draft recommended continuous tests, described in section 6.5.1.2.2.
    // That test runs over a window of 65536 8-bit samples rather than the
    // blocks of 2500 samples the FIPS 140-2 tests use, so its results are
    // accumulated persistently over several analysis blocks.
    class FIPS
    { //{{{
    public:

        // The FIPS 140-2 tests are designed for blocks of 20000 bits
        static const unsigned   BUFFER_SIZE = 2500;

        // Q20 resolution, Short term period 1000 samples
        typedef QA::FailRate< 20, 1000 >    FailRate;

        // Q14 resolution, Short term period 10 runs
        typedef QA::PassRuns< 14, 10 >      PassRuns;


        enum Test
        {
            MONOBIT     = 0,
            POKER       = 1,
            RUNS        = 2,
            LONG_RUN    = 3,
            REPETITION  = 4,
            PROPORTION  = 5,
            RESULT,
            TEST_MAX
        };

        static const char *TestName( Test n )
        { //{{{

            switch( n )
            {
                case MONOBIT:       return "Monobit";
                case POKER:         return "Poker";
                case RUNS:          return "Runs";
                case LONG_RUN:      return "Long run";
                case REPETITION:    return "Repetition";
                case PROPORTION:    return "Proportion";
                case RESULT:        return "Result";
                case TEST_MAX:      break;
            }
            return "Unknown test";

        } //}}}


    private:

        // The 'Continuous test' for 16-bit words really isn't very useful.
        // We'd expect to get C(65536,1) * p(x)^2 * p(not_x) * 1250 twins
        // in each block (a pair in about 1.9% of blocks) from a uniformly
        // random source.  Since FIPS-140-2 4.9.2 says this test fails if
        // any consecutive blocks are equal regardless of the chosen n, we
        // would fail far too many otherwise good blocks if using n = 16.
        //
        // For n = 32 that should become more like a 0.00001455% chance,
        // so 1 block in about 6.87 million should fail this test (as the
        // long term average for many billions of blocks).  Assuming that
        // C(1^32,1) * p(1/(1^32))^2 * p(((1^32)-1)/(1^32)) * 625
        // is a fair estimation of that probability.
        //
        // A better version of this test is described in the NIST SP 800-90B
        // 2012 draft, in section 6.5.1.2.1 Repetition Count Test. That test
        // defined an expected failure rate based on the assessed min-entropy
        // of the generator when operating correctly, but it's still a weak
        // test since a small degradation in real entropy can take quite a
        // long time to register as a test failure. The FIPS Continuous Test
        // we do here for n=32 is equivalent to that for min-entropy H > 30
        // bits per sample.
      //uint16_t        m_previous_word;

        uint32_t        m_previous_word;
        unsigned        m_pokerbins[16];

        // This one has a dummy extra slot to avoid needing a test
        // for (not counting) the very first bit in the hot loop.
        unsigned        m_runs[3][6];

        // We need to keep persistent counts for the Adaptive Proportion test
        // since it operates with a different block size to what the rest of
        // the FIPS 140-2 tests use.
        uint8_t         m_prop_val;
        unsigned        m_prop_count;
        unsigned        m_prop_n;

        FailRate        m_failrate[ TEST_MAX ];
        PassRuns        m_passruns[ TEST_MAX ];

        BitRuns         m_bitruns;


    public:

        FIPS()
            : m_previous_word( 0x5EED1E57 )  // chosen by a fair dice roll
            , m_prop_val( 0 )
            , m_prop_count( 0 )
            , m_prop_n( 65535 )
        {
            Log<2>( "+ FIPS\n" );
        }

        FIPS( const Json::Data::Handle &fips )
        { //{{{

            Log<2>( "+ FIPS( json )\n" );

            for( unsigned i = 0; i < TEST_MAX; ++i )
            {
                Json::Data::Handle  test = fips[TestName(Test(i))];

                m_failrate[i] = FailRate( test["FailRate"] );
                m_passruns[i] = PassRuns( test["PassRuns"] );
            }

        } //}}}

        ~FIPS()
        {
            Log<2>( "- FIPS\n" );
        }


        unsigned Check( const uint8_t *buf )
        { //{{{

            const unsigned pokermin = unsigned(( 2.16 + 5000.) * 5000. / 16.);  // 1563175
            const unsigned pokermax = unsigned((46.17 + 5000.) * 5000. / 16.);  // 1576928.125

            unsigned result     = 0;
            unsigned ones_count = 0;
            unsigned run_length = 0;
            unsigned run_bit    = 2;
            unsigned word       = 0;
            unsigned word_byte  = 0;

            memset( m_pokerbins, 0, sizeof(m_pokerbins) );
            memset( m_runs, 0, sizeof(m_runs) );


            for( unsigned i = 0; i < BUFFER_SIZE; ++i )
            {
                word = word << 8 | buf[i];

                if( ++word_byte == sizeof(m_previous_word) )
                {
                    if( m_previous_word == word )
                        result |= 1u << REPETITION;

                    m_previous_word  = word;
                    ones_count += popcount( word );
                    word        = 0;
                    word_byte   = 0;
                }

                ++m_pokerbins[ buf[i] >> 4 ];
                ++m_pokerbins[ buf[i] & 0xf ];

                for( int b = 7; b >= 0; --b )
                {
                    unsigned    bit = buf[i] & 1u << b ? 1 : 0;
                    if( bit == run_bit )
                    {
                        ++run_length;
                        continue;
                    }

                    if( run_length > 5 )
                    {
                        if( run_length >= 25 )
                            result |= 1u << LONG_RUN;

                        ++m_runs[run_bit][5];
                    }
                    else
                        ++m_runs[run_bit][run_length];

                    if( __builtin_expect(run_bit != 2, 1) )
                        m_bitruns.AddBits( run_bit, run_length + 1 );

                    run_bit    = bit;
                    run_length = 0;
                }

                // We assume min-entropy H = 8 bits for this test, and perform
                // it on a window of 65536 samples.  The cutoff is chosen for
                // a 2^-30 chance of reporting a false positive.
                if( m_prop_val == buf[i] && ++m_prop_count > 358 )
                {
                    result |= 1u << PROPORTION;
                    goto reset_prop_count;
                }

                if( ++m_prop_n >= 65536 )
                {
                    reset_prop_count:
                    m_prop_val          = buf[i];
                    m_prop_count        = 0;
                    m_prop_n            = 0;
                }
            }

            if( run_length > 5 )
            {
                if( run_length >= 25 )
                    result |= 1u << LONG_RUN;

                ++m_runs[run_bit][5];
            }
            else
                ++m_runs[run_bit][run_length];

            m_bitruns.AddBits( run_bit, run_length + 1 );


            if( ones_count <= 9725 || ones_count >= 10275 )
                result |= 1u << MONOBIT;


            size_t  pokersum = 0;

            for( unsigned i = 0; i < 16; ++i )
                pokersum += m_pokerbins[i] * m_pokerbins[i];

            if( pokersum <= pokermin || pokersum > pokermax )
                result |= 1u << POKER;


            for( unsigned i = 0; i < 2; ++i )
            {
                if( m_runs[i][0] < 2315 || m_runs[i][0] > 2685
                 || m_runs[i][1] < 1114 || m_runs[i][1] > 1386
                 || m_runs[i][2] <  527 || m_runs[i][2] >  723
                 || m_runs[i][3] <  240 || m_runs[i][3] >  384
                 || m_runs[i][4] <  103 || m_runs[i][4] >  209
                 || m_runs[i][5] <  103 || m_runs[i][5] >  209 )
                {
                    result |= 1u << RUNS;
                    break;
                }
            }

            return result ? 1u << RESULT | result : 0;

        } //}}}

        bool Analyse( const uint8_t *buf )
        { //{{{

            unsigned result = Check( buf );

            // The Pass counts for the Adaptive Proportion test will be a
            // bit skewed here, since with 64kB blocks we don't really pass
            // one for sure with every block processed.  In the worst case,
            // we could fail one for every 2500 byte block though. It's not
            // really worth special casing for that though, since the rates
            // of (false positive) failure that are expected for that test
            // are so much lower than for the rest of them that being off by
            // even a factor of 26 in a moving average window of 1000 tests
            // isn't going to make any notable difference to our ability to
            // trigger on it.  Graphing the absolute count is a much more
            // interesting statistic for that one.  If it fires more often
            // than expected, the Chi-squared metric will probably trip to
            // warn us first in any case, if no other test does first.
            for( unsigned i = 0; i < TEST_MAX; ++i )
            {
                if( result & 1u << i )
                {
                    m_failrate[i].Fail();
                    m_passruns[i].Fail();
                } else {
                    m_failrate[i].Pass();
                    m_passruns[i].Pass();
                }
            }

            return result ? false : true;

        } //}}}


        bool IsOk( bool was_ok = true ) const
        { //{{{

            // The runlength maximums are selected with the following rationale:
            // A maximum pass runlength of 17500 or longer is expected to occur
            // about once in every 1.17 million runs (about 3.4 TB of samples).
            // If the generator is running constantly at a 1 Mbps rate, we can
            // detect if a run is longer than that after about 350 seconds, and
            // would expect this to happen about once every 10 months.  It will
            // take (possibly much) longer to detect an excess success anomaly
            // in any of the individual tests, but that would also tend to bias
            // the overall success rate toward excess unless one of the other
            // tests was also failing more often than expected, both of which
            // are likely to increase the chance of detecting it early through
            // some related measure.  Tracking them individually does give us
            // a longer term gauge that could still be useful though.
            //
            // If the generator is run at a considerably higher or lower rate
            // then it might be worth tuning these to suit that better, but
            // this isn't a very probable failure mode in real world use. The
            // generator itself is unlikely to fail into such a mode, and any
            // influence that an external attacker may be able to have is not
            // likely to trigger this either (if they are capable of injecting
            // known data that passes the fail tests, it would be much easier
            // to ensure it passes this test too).  Mostly this is a test of
            // of the software performing the tests.  If we see an excessive
            // run without any failure, then maybe that's a bug in the test.
            //
            // There's not much point to tracking the pass runlengths of the
            // Adaptive Proportion test.  It has a design false positive rate
            // of around one in 2^-30, so its possibly legitimate limit for
            // pass runlengths is near enough to infinite for our purposes.
            //
            // p(<=maxpass) = Sum(n<=maxpass, p(pass)^n * p(fail))
            static const size_t maxpass[TEST_MAX] =
            {
                134500,     // one in 1189248 runs is expected to be longer than 134500,
                141200,     // one in 1178310 runs is expected to be longer than 141200
                42500,      // one in 1135535 runs is expected to be longer than 42500
                46900,      // one in 1177119 runs is expected to be longer than 46900
                96000000,   // one in 1164281 runs is expected to be longer than 96000000
                size_t(-1), // one in 41 runs is expected to be longer than 4,000,000,000
                17500       // one in 1170399 runs is expected to be longer than 17500
            };

            // We're actually somewhat less likely to hit these limits, purely
            // by chance, than what is indicated below, since we are measuring
            // them against a modified moving average with alpha of 0.001, so
            // a failure that occurred 1000 samples ago only counts as 37% of
            // one, while a failure that occurred 5000 samples ago still counts
            // for 0.6% of a measured tally.  The probabilities below show the
            // chance of getting more than that number of failures in exactly
            // 1000 trials. Since both more failures are needed to get to that
            // count, and we're measuring them over a longer trial period, we
            // can't really define a single precise cutoff rate since it will
            // depend on not just their average rate, but how they are grouped
            // together too.  It does have the desirable property of weighting
            // a suddenly high rate of failures more heavily, and discounting
            // older failures during a recovery period, and the rates are low
            // enough that any catastrophic failure will quickly exceed them.
            // For real world use that seems like a more valuable measure than
            // offering a precise statistical significance as the cutoff for
            // shutting down output in the event of detected failure.  If any
            // use case does demand a more rigorous bound, then it would need
            // to be measured against a simple moving average instead.  We'll
            // worry about that if somebody actually makes the case for it as
            // being the preferred behaviour in some or all instances.
            //
            // p(>maxfail) = 1 - Sum(n<=maxfail, C(1000,n) * p(fail)^n * p(pass)^(1000-n))
            static const size_t maxfail[TEST_MAX] =
            {
                // one in 10858504 chance of > 4 monobit failures per 1000 blocks
                size_t(0.004 * (1u << FailRate::AVG_Q)),

                // one in 13834536 chance of > 4 poker failures per 1000 blocks
                size_t(0.004 * (1u << FailRate::AVG_Q)),

                // one in 16747771 chance of > 6 runs failures per 1000 blocks
                size_t(0.006 * (1u << FailRate::AVG_Q)),

                // one in 31936421 chance of > 6 long run failures per 1000 blocks
                size_t(0.006 * (1u << FailRate::AVG_Q)),

                // one in 94575485 chance of > 1 repetition failures per 1000 blocks
                size_t(0.001 * (1u << FailRate::AVG_Q)),

                // one in 2307763068086 chance of > 1 proportion failures per 1000 blocks
                size_t(0.001 * (1u << FailRate::AVG_Q)),

                // one in 507761 chance of > 7 failures of any kind per 1000 blocks
                size_t(0.007 * (1u << FailRate::AVG_Q))
            };

            if( was_ok )
            {
                for( unsigned i = 0; i < TEST_MAX; ++i )
                {
                    if( m_failrate[i].rate > maxfail[i] )
                        return false;

                    if( m_passruns[i].Current() > maxpass[i] )
                        return false;
                }

            } else {

                // Be conservative about declaring recovery from failure here.
                // Wait until we're operating well within the defined bounds again.
                // If everything is operating normally, and the bounds are set well,
                // we'd expect to fail this test reasonably often in normal running,
                // but not so often that it's unreasonable to expect to be within
                // these bounds for some period again before resuming normal service.

                // At the very least the last 20 blocks must have all passed.
                // This is mostly just to avoid the case where the source produces
                // bad results right from its very first output, but we haven't yet
                // analysed enough blocks from it to trip the average failure rate
                // thresholds.  The pathological case of that being we only read one
                // one block before calling this function, and it fails, giving us
                // a running average of 1 failure per 1000, which is well within the
                // expected rates of failure for almost all tests.  This should pass
                // with near certain probability if the source is functioning ok, we
                // expect average pass runs of about 1250 blocks between failures.

                if( m_passruns[RESULT].Current() < 20 )
                    return false;

                for( unsigned i = 0; i < TEST_MAX; ++i )
                {
                    if( m_failrate[i].rate > maxfail[i] / 2 )
                        return false;

                    // We test against the previous runlength here, since the
                    // current one may be under the limit because it is still
                    // in progress and we don't know what its length will be.
                    if( maxpass[i] != size_t(-1)
                     && m_passruns[i].Previous() > maxpass[i] / 2 )
                        return false;
                }
            }

            return true;

        } //}}}


        const FailRate &GetFailRate( Test n = RESULT ) const
        {
            return m_failrate[n];
        }

        const PassRuns &GetPassRuns( Test n = RESULT ) const
        {
            return m_passruns[n];
        }


        std::string ReportFailRates() const
        { //{{{

            std::string     s;

            s += stringprintf( _("Fail rate: %zu / %zu %.3f %.3f %.3f"), m_failrate[RESULT].fail,
                                               m_failrate[RESULT].pass + m_failrate[RESULT].fail,
                                               m_failrate[RESULT].ShortTerm() * 1000.0,
                                               m_failrate[RESULT].LongTerm() * 1000.0,
                                               m_failrate[RESULT].Peak() * 1000.0 );

            for( unsigned i = 0; i < RESULT; ++i )
                if( m_failrate[i].fail )
                    s += stringprintf( ", %s: %zu %.3f %.3f %.3f", TestName( Test(i) ),
                                                    m_failrate[i].fail,
                                                    m_failrate[i].ShortTerm() * 1000.0,
                                                    m_failrate[i].LongTerm() * 1000.0,
                                                    m_failrate[i].Peak() * 1000.0 );
            return s;

        } //}}}

        std::string ReportPassRuns() const
        { //{{{

            std::string     s;

            s += stringprintf( _("Pass runs: %zu %zu %zu %zu"), m_passruns[RESULT].Runs(),
                                                                m_passruns[RESULT].ShortTerm(),
                                                                m_passruns[RESULT].LongTerm(),
                                                                m_passruns[RESULT].Peak() );
            for( unsigned i = 0; i < RESULT; ++i )
                if( m_passruns[i].Runs() )
                    s += stringprintf( ", %s: %zu %zu %zu %zu", TestName( Test(i) ),
                                                                m_passruns[i].Runs(),
                                                                m_passruns[i].ShortTerm(),
                                                                m_passruns[i].LongTerm(),
                                                                m_passruns[i].Peak() );
            return s;

        } //}}}


        std::string ResultsAsJSON() const
        { //{{{

            std::string     s = "\"FIPS\":{";

            for( unsigned i = 0; i < TEST_MAX; ++i )
            {
                if( i )
                    s += ',';

                s += stringprintf( "\"%s\":{", TestName( Test(i) ) );

                s +=  "\"PassRuns\":" + m_passruns[i].AsJSON();
                s += ",\"FailRate\":" + m_failrate[i].AsJSON();

                s += '}';
            }

            s += '}';  // FIPS

            s += ",\"BitRuns\":" + m_bitruns.GetResult().AsJSON();

            return s;

        } //}}}

    }; //}}}

  } // QA namespace

}   // BitB namespace


#endif  // _BB_QA_H

// vi:sts=4:sw=4:et:foldmethod=marker
