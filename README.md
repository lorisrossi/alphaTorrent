# αTorrent
A simple BitTorrent Client.

Project for the Computer Networks course held by A. Mancini at Univpm

###Troubleshooting

If you are on a OS X system and the compiler throw this error:
> fatal error: ‘openssl/sha.h’ file not found

These commands should solve the problem:
```bash
brew install openssl
cd /usr/local/include 
ln -s ../opt/openssl/include/openssl .
```