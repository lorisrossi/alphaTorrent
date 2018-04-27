# αTorrent
A simple BitTorrent Client.

Project for the Computer Networks course held by A. Mancini at Univpm

### Dependencies

The client needs OpenSSL and Boost libraries.

**Mac OS X**

OpenSSL
```bash
brew install openssl
cd /usr/local/include 
ln -s ../opt/openssl/include/openssl .
```

Boost
```bash
brew install boost
```

### Troubleshooting

For OS X users who have this error:
> fatal error: ‘openssl/sha.h’ file not found

please install the OpenSSL library as showed above
