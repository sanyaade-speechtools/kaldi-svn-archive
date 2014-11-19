#!/bin/bash
# Copyright  2014 Johns Hopkins University (Minhua Wu)
#
# convert UW data to standard text form for language model training

TRANS_DIR=$1
mkidr -p $TRANS_DIR

echo 'download and decode uw data to utf-8 transcripts'
wget --no-check-certificate -P $TRANS_DIR \
  http://ssli.ee.washington.edu/data/100M_conv_mandarin-ppl-filt.gz
gunzip $TRANS_DIR/100M_conv_mandarin-ppl-filt.gz
iconv -c -f GBK -t utf-8 $TRANS_DIR/100M_conv_mandarin-ppl-filt \
  > $TRANS_DIR/transcripts.txt

echo 'text filtering ...'
cat $TRANS_DIR/transcripts.txt |\
  sed -e 's/*//g' |\
  sed -e 's/&//g' |\
  sed -e '/[%$#=@]/d' |\
  awk '{if (NF > 1) {print "0000 ",$0;}}' |\
  uconv -f utf-8 -t utf-8 -x "Any-Upper" > $TRANS_DIR/transcripts_filtered.txt

# Express digit numbers in Chinese characters    
echo 'digit normalization...'
python local/num_dates_char.py \
  -i $TRANS_DIR/transcripts_filtered.txt -o $TRANS_DIR/transcripts_normalized.txt  


pyver=`python --version 2>&1 | sed -e 's:.*\([2-3]\.[0-9]\+\).*:\1:g'`
export PYTHONPATH=$PYTHONPATH:`pwd`/tools/mmseg-1.3.0/lib/python${pyver}/site-packages
if [ ! -d tools/mmseg-1.3.0/lib/python${pyver}/site-packages ]; then
  echo "--- Downloading mmseg-1.3.0 ..."
  echo "NOTE: it assumes that you have Python, Setuptools installed on your system!"
  wget -P tools http://pypi.python.org/packages/source/m/mmseg/mmseg-1.3.0.tar.gz
  tar xf tools/mmseg-1.3.0.tar.gz -C tools
  cd tools/mmseg-1.3.0
  mkdir -p lib/python${pyver}/site-packages
    python setup.py build
  python setup.py install --prefix=.
  cd ../..
  if [ ! -d tools/mmseg-1.3.0/lib/python${pyver}/site-packages ]; then
    echo "mmseg is not found - installation failed?"
    exit 1
  fi
fi

# segment sentences using mmseg 
cat $TRANS_DIR/transcripts_normalized.txt |\
  python local/callhome_mmseg_segment.py > $TRANS_DIR/text
