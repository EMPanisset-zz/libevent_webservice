git clone https://github.com/libevent/libevent.git

git clone https://github.com/akheron/jansson.git

git clone https://github.com/nodejs/http-parser.git

(cd libevent; git checkout release-2.1.8-stable)

(cd jansson; git checkout v2.11)

(cd http-parser; git checkout v2.8.0)

patch -d http-parser -p1 <webservice/http-parser.patch
