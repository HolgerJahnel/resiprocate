#ifdef HAVE_CONFIG_H
#include <config.hxx>
#endif

#if HAVE_POPT_H
#include <popt.h>
#endif

#include "CommandLineParser.hxx"
#include "rutil/Logger.hxx"
#include "rutil/DnsUtil.hxx"
#include "resip/stack/ParseException.hxx"

using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM Subsystem::TEST

CommandLineParser::CommandLineParser(int argc, char** argv)
{
   char* logType = "cout";
   char* logLevel = "INFO";
   char* tlsDomain = 0;
   int udpPort = 5060;
   int tcpPort = 5060;
#if defined(USE_SSL)
   int tlsPort = 5061;
#else
   int tlsPort = 0;
#endif
   int dtlsPort = 0;
   int disableV4 = false;
   int enableV6 = false;
   char* domains = 0;
   char* interfaces = 0;
   char* routeSet = 0;
   char certPathBuf[256];
   char* certPath = certPathBuf;
   int noChallenge = false;
   int noWebChallenge = false;
   
   int noRegistrar = false;
   int certServer = true;
   int shouldRecordRoute = false;

   char* reqChainName = "default";
   char* mySqlServer = 0;
   int httpPort = 5080;
   int recursiveRedirect = 0;
   int doQValue=0;
   char* enumSuffix = 0;
   int allowBadReg = 0;
   int timerC=180;
   
#ifdef WIN32
#ifndef HAVE_POPT_H
   noChallenge = 1;  // If no POPT, then default to no digest challenges
#endif
   strcpy(certPath,"C:\\sipCerts");   
#else
   strcpy(certPath, getenv("HOME"));
   strcat(certPath, "/.sipCerts");
#endif

#ifdef HAVE_POPT_H
   struct poptOption table[] = {
      {"log-type",         'l',  POPT_ARG_STRING| POPT_ARGFLAG_SHOW_DEFAULT, &logType,        0, "where to send logging messages", "syslog|cerr|cout"},
      {"log-level",        'v',  POPT_ARG_STRING| POPT_ARGFLAG_SHOW_DEFAULT, &logLevel,       0, "specify the default log level", "STACK|DEBUG|INFO|WARNING|ALERT"},
      {"record-route",     'r',  POPT_ARG_NONE,                        &shouldRecordRoute,    0, "specify whether to record route", 0},
#if defined(USE_MYSQL)
      {"mysqlServer",      'x',  POPT_ARG_STRING| POPT_ARGFLAG_SHOW_DEFAULT, &mySqlServer,    0, "enable MySQL and provide name of server", "localhost"},
#endif
      {"udp",                0,  POPT_ARG_INT| POPT_ARGFLAG_SHOW_DEFAULT,    &udpPort,        0, "add UDP transport on specified port", "5060"},
      {"tcp",                0,  POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT,   &tcpPort,        0, "add TCP transport on specified port", "5060"},
#if defined(USE_SSL)
      {"tls-domain",       't',  POPT_ARG_STRING,                            &tlsDomain,      0, "act as a TLS server for specified domain", "example.com"},
      {"tls",                0,  POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT,   &tlsPort,        0, "add TLS transport on specified port", "5061"},
      {"dtls",               0,  POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT,   &dtlsPort,       0, "add DTLS transport on specified port", "0"},
      {"enable-cert-server", 0,  POPT_ARG_NONE,                              &certServer,     0, "run a cert server", 0},
#ifdef WIN32
      {"cert-path",        'c',  POPT_ARG_STRING| POPT_ARGFLAG_SHOW_DEFAULT, &certPath,       0, "path for certificates (default: c:\\sipCerts)", 0},
#else
      {"cert-path",        'c',  POPT_ARG_STRING| POPT_ARGFLAG_SHOW_DEFAULT, &certPath,       0, "path for certificates (default: ~/.sipCerts)", 0},
#endif
#endif
      {"enable-v6",         0,   POPT_ARG_NONE,                              &enableV6,       0, "enable IPV6", 0},
      {"disable-v4",        0,   POPT_ARG_NONE,                              &disableV4,      0, "disable IPV4", 0},
      {"disable-auth",      0,   POPT_ARG_NONE,                              &noChallenge,    0, "disable DIGEST challenges", 0},
      {"disable-web-auth",  0,   POPT_ARG_NONE,                              &noWebChallenge, 0, "disable HTTP challenges", 0},
      {"disable-reg",       0,   POPT_ARG_NONE,                              &noRegistrar,    0, "disable registrar", 0},
      {"interfaces",      'i',   POPT_ARG_STRING,                            &interfaces,     0, "specify interfaces to add transports to", "sip:10.1.1.1:5065;transport=tls"},
      {"domains",         'd',   POPT_ARG_STRING,                            &domains,        0, "specify domains that this proxy is authorative", "example.com,foo.com"},
      {"route",           'R',   POPT_ARG_STRING,                            &routeSet,       0, "specify where to route requests that are in this proxy's domain", "sip:p1.example.com,sip:p2.example.com"},
      {"reqChainName",      0,   POPT_ARG_STRING,                            &reqChainName,   0, "name of request chain (default: default)", 0},
      {"http",              0,   POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT,   &httpPort,       0, "run HTTP server on specified port", "5080"},
      {"recursive-redirect",0,   POPT_ARG_NONE,                              &recursiveRedirect, 0, "Handle 3xx responses in the proxy", 0},
      {"q-value",0,   POPT_ARG_NONE,                              &doQValue, 0, "Enable sequential q-value processing", 0},
      {"enum-suffix",     'e',   POPT_ARG_STRING,                            &enumSuffix,     0, "specify enum suffix to search", "e164.arpa"},
      {"allow-bad-reg",   'b',   POPT_ARG_NONE,                              &allowBadReg,    0, "allow To tag in registrations", 0},
      {"timer-C",          0,    POPT_ARG_INT,                                &timerC,          0, "specify length of timer C in sec (0 or negative will disable timer C)", "180"},
      POPT_AUTOHELP 
      { NULL, 0, 0, NULL, 0 }
   };
   
   poptContext context = poptGetContext(NULL, argc, const_cast<const char**>(argv), table, 0);
   if (poptGetNextOpt(context) < -1)
   {
      cerr << "Bad command line argument entered" << endl;
      poptPrintHelp(context, stderr, 0);
      exit(-1);
   }
#endif

   mHttpPort = httpPort;
   mLogType = logType;
   mLogLevel = logLevel;

   if (tlsDomain) 
   {
      mTlsDomain = tlsDomain;
   }

   mShouldRecordRoute = shouldRecordRoute;

   mUdpPort = udpPort;
   mTcpPort = tcpPort;
   mTlsPort = tlsPort;
   mDtlsPort = dtlsPort;
   mUseV4 = !disableV4;
   mUseV6 = enableV6?true:false;
   mInterfaces = toVector(interfaces, "interfaces"); 
   mDomains = toVector(domains, "domains"); 
   mRouteSet = toVector(routeSet, "routeSet"); 
   mCertPath = certPath;
   mNoChallenge = noChallenge != 0;
   mNoWebChallenge = noWebChallenge != 0;
   mNoRegistrar = noRegistrar != 0 ;
   mCertServer = certServer !=0 ;
   mRequestProcessorChainName=reqChainName;
   mRecursiveRedirect = recursiveRedirect?true:false;
   mDoQValue = doQValue?true:false;
   mAllowBadReg = allowBadReg?true:false;
   if (enumSuffix) mEnumSuffix = enumSuffix;
   
   if (mySqlServer) 
   {
      mMySqlServer = Data(mySqlServer);
   }
   
   if(timerC >0)
   {
      mTimerC=timerC;
   }
   else
   {
      mTimerC=0;
   }

#ifdef HAVE_POPT_H
   poptFreeContext(context);
#endif
}

resip::Uri 
CommandLineParser::toUri(const char* input, const char* description)
{
   resip::Uri uri;
   try
   {
      if (input)
      {
         uri = Uri(input);
      }
      else
      {
         std::cerr << "No " << description << " specified" << std::endl;
      }
   } 
   catch (ParseException& e)
   {
      std::cerr << "Caught: " << e << std::endl;
      std::cerr << "Can't parse " << description << " : " << input << std::endl;
      exit(-1);
   }
   return uri;
}

std::vector<resip::Data> 
CommandLineParser::toVector(const char* input, const char* description)
{
   std::vector<Data> domains; 

   if (input)
   {
      Data buffer = input;
      if (input)
      {
         for (char* token = strtok(const_cast<char*>(buffer.c_str()), ","); token != 0; token = strtok(0, ","))
         {
            try
            {
               domains.push_back(token);
            } 
            catch (ParseException& e)
            {
               std::cout << "Caught: " << e << std::endl;
               std::cerr << "Can't parse " << description << " : " << token << std::endl;
               exit(-1);
            }
            catch (...)
            {
               std::cout << "Caught some exception" <<std::endl;
               std::cerr << "Some problem parsing " << description << " : " << token << std::endl;
            }
         }
      }
   }
   return domains;
}
   