#include "bass.h"

#define BASSFFMPEGDEF(f) WINAPI f

// BASS_CHANNELINFO type
#define BASS_CTYPE_STREAM_FFMPEG 0x1f301

// File extensions extracted from all AVOutputFormat
#define BASS_EXTENSIONS_FFMPEG "*.264;*.265;*.302;*.3g2;*.3gp;*.669;*.722;*.aa;*.aa3;*.aac;*.abc;*.ac3;*.acm;*.adf;*.adp;*.ads;*.adx;*.aea;*.afc;*.aix;*.al;*.amf;*.ams;*.ans;*.ape;*.apl;*.aptx;*.aptxhd;*.aqt;*.art;*.asc;*.ast;*.avc;*.avi;*.avr;*.avs;*.bcstm;*.bfstm;*.bit;*.bmv;*.brstm;*.c2;*.cdata;*.cdg;*.cdxl;*.cgi;*.cif;*.daud;*.dbm;*.dif;*.diz;*.dmf;*.dsm;*.dss;*.dtk;*.dts;*.dtshd;*.dv;*.eac3;*.fap;*.far;*.flac;*.flm;*.flv;*.fsb;*.g722;*.g723_1;*.g729;*.genh;*.gsm;*.h261;*.h264;*.h265;*.h26l;*.hevc;*.ice;*.idf;*.idx;*.ircam;*.it;*.itgz;*.itr;*.itz;*.ivr;*.j2k;*.lvf;*.m2a;*.m4a;*.m4v;*.mac;*.mdgz;*.mdl;*.mdr;*.mdz;*.med;*.mid;*.mj2;*.mjpeg;*.mjpg;*.mk3d;*.mka;*.mks;*.mkv;*.mlp;*.mod;*.mov;*.mp2;*.mp3;*.mp4;*.mpa;*.mpc;*.mpl2;*.mpo;*.msbc;*.msf;*.mt2;*.mtaf;*.mtm;*.musx;*.mvi;*.mxg;*.nfo;*.nist;*.nsp;*.nut;*.ogg;*.okt;*.oma;*.omg;*.paf;*.pjs;*.psm;*.ptm;*.pvf;*.qcif;*.rco;*.rgb;*.rsd;*.rso;*.rt;*.s3gz;*.s3m;*.s3r;*.s3z;*.sami;*.sb;*.sbc;*.sbg;*.scc;*.sdr2;*.sds;*.sdx;*.sf;*.shn;*.sln;*.smi;*.son;*.sph;*.ss2;*.stl;*.stm;*.str;*.sub;*.sup;*.svag;*.sw;*.tak;*.tco;*.thd;*.tta;*.txt;*.ty;*.ty+;*.ub;*.ul;*.ult;*.umx;*.uw;*.v;*.v210;*.vag;*.vb;*.vc1;*.viv;*.vpk;*.vqe;*.vqf;*.vql;*.vt;*.vtt;*.wsd;*.xl;*.xm;*.xmgz;*.xmr;*.xmv;*.xmz;*.xvag;*.y4m;*.yop;*.yuv;*.yuv10"

HSTREAM BASSFFMPEGDEF(BASS_FFMPEG_StreamCreateFile)(DWORD filetype, const void* file, QWORD offset, QWORD length, DWORD flags);
