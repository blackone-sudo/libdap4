
# $Id: urls.tcl,v 1.1 1997/08/29 22:02:30 jimg Exp $

# Datasets and their expected output (the information that writeval sends to
# stdout - not the stuff that should be going into the file).

# URI/BEARS:
set bears "http://$host/test/nph-nc/data/bears.nc"
set bears_ce "bears"
set bears_dds "Dataset {
    Grid {
     ARRAY:
        String bears\[i = 2\]\[j = 3\]\[l = 3\];
     MAPS:
        Int32 i\[i = 2\];
        Float64 j\[j = 3\];
        Int32 l\[l = 3\];
    } bears;
    Grid {
     ARRAY:
        Int32 order\[i = 2\]\[j = 3\];
     MAPS:
        Int32 i\[i = 2\];
        Float64 j\[j = 3\];
    } order;
    Grid {
     ARRAY:
        Int32 shot\[i = 2\]\[j = 3\];
     MAPS:
        Int32 i\[i = 2\];
        Float64 j\[j = 3\];
    } shot;
    Grid {
     ARRAY:
        Float64 aloan\[i = 2\]\[j = 3\];
     MAPS:
        Int32 i\[i = 2\];
        Float64 j\[j = 3\];
    } aloan;
    Grid {
     ARRAY:
        Float64 cross\[i = 2\]\[j = 3\];
     MAPS:
        Int32 i\[i = 2\];
        Float64 j\[j = 3\];
    } cross;
    Int32 i\[i = 2\];
    Float64 j\[j = 3\];
    Int32 l\[l = 3\];
} bears;"

# URI/FNOC
set fnoc1 "http://$host/test/nph-nc/data/fnoc1.nc"
set fnoc1_ce "u\\\[0:0\\\]\\\[0:9\\\]\\\[0:9\\\]"
set fnoc1_dds "Dataset {
    Int32 u\[time_a = 16\]\[lat = 17\]\[lon = 21\];
    Int32 v\[time_a = 16\]\[lat = 17\]\[lon = 21\];
    Float64 lat\[lat = 17\];
    Float64 lon\[lon = 21\];
    Float64 time\[time = 16\];
} fnoc1;"

# URI/DSP:
set dsp_1 "http://$host/test/nph-dsp/data/f96243170857.img"
set dsp_1_ce "dsp_band_1\\\[20:30\\\]\\\[20:30\\\]"
set dsp_1_dds "Dataset {
    Grid {
     ARRAY:
        Byte dsp_band_1\[lat = 512\]\[lon = 512\];
     MAPS:
        Float64 lat\[lat = 512\];
        Float64 lon\[lon = 512\];
    } dsp_band_1;
    Float64 lat\[lat = 512\];
    Float64 lon\[lon = 512\];
} f96243170857;"

# URI/MatLab:
set nscat_s2 "http://$host/test/nph-mat/data/NSCAT_S2.mat"
set nscat_s2_ce "NSCAT_S2\\\[75:75\\\]\\\[0:5\\\]"
set nscat_s2_dds "Dataset {
    Float64 NSCAT_S2\[NSCAT_S2_row = 153\]\[NSCAT_S2_column = 843\];
} NSCAT_S2;"

# URI/NSCAT:
set nscat_hdf "http://$host/test/nph-hdf/data/S2000415.HDF"
set nscat_hdf_ce "WVC_Lat\\\[200:201\\\]\\\[20:21\\\]"
set nscat_hdf_dds "Dataset {
    Int32 WVC_Lat\[row = 458\]\[WVC = 24\];
    UInt32 WVC_Lon\[row = 458\]\[WVC = 24\];
    UInt32 Num_Sigma0\[row = 458\]\[WVC = 24\];
    UInt32 Num_Beam_12\[row = 458\]\[WVC = 24\];
    UInt32 Num_Beam_34\[row = 458\]\[WVC = 24\];
    UInt32 Num_Beam_56\[row = 458\]\[WVC = 24\];
    UInt32 Num_Beam_78\[row = 458\]\[WVC = 24\];
    UInt32 WVC_Quality_Flag\[row = 458\]\[WVC = 24\];
    UInt32 Mean_Wind\[row = 458\]\[WVC = 24\];
    UInt32 Wind_Speed\[row = 458\]\[WVC = 24\]\[position = 4\];
    UInt32 Wind_Dir\[row = 458\]\[WVC = 24\]\[position = 4\];
    UInt32 Error_Speed\[row = 458\]\[WVC = 24\]\[position = 4\];
    UInt32 Error_Dir\[row = 458\]\[WVC = 24\]\[position = 4\];
    Int32 MLE_Likelihood\[row = 458\]\[WVC = 24\]\[position = 4\];
    UInt32 Num_Ambigs\[row = 458\]\[WVC = 24\];
    Sequence {
        Structure {
            Int32 begin__0;
        } begin;
    } SwathIndex;
    Sequence {
        Structure {
            String Mean_Time__0;
        } Mean_Time;
        Structure {
            UInt32 Low_Wind_Speed_Flag__0;
        } Low_Wind_Speed_Flag;
        Structure {
            UInt32 High_Wind_Speed_Flag__0;
        } High_Wind_Speed_Flag;
    } NSCAT%20L2;
} S2000415%2eHDF;"

# URI/JGOFS:
set jg_test "http://$host/test/nph-jg/test"
set jg_test_ce "year,month,lat,lon,sal&lat>37.5&lat<38.0"
set jg_test_dds "Dataset {
    Sequence {
        String leg;
        String year;
        String month;
        Sequence {
            String station;
            String lat;
            String lon;
            Sequence {
                String press;
                String temp;
                String sal;
                String o2;
                String sigth;
            } Level_2;
        } Level_1;
    } Level_0;
} test;"



