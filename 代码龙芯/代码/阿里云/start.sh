#!/bin/bash

#Check Evironment
os=`uname -s`

if [ "$os" != "Linux" ]; then
    echo "Error: OS $os is unsupported"
    exit 1
fi

if [ "`uname -m`" = "x86_64" ]; then
    if [ "`which apt-get`" != "" ]; then
        if [ "`which gcc`" = "" ] || [ "`which make`" = "" ]; then
            sudo apt-get -y install build-essential
        fi 
        if [ "`which tar`" = "" ]; then
            sudo apt-get -y install tar
        fi
        if [ "`which sed`" = "" ]; then
            sudo apt-get -y install sed 
        fi
        if [ "`which awk`" = "" ]; then
            sudo apt-get -y install awk
        fi
        if [ "`which cmake`" = "" ]; then
            sudo apt-get -y install cmake 
        fi
        if [ "`which wget`" = "" ]; then
            sudo apt-get -y install wget
        fi
    elif [ "which yum" != "" ]; then
        if [ "`which gcc`" = "" ] || [ "`which make`" = "" ]; then
            echo "gcc and make are need"
            exit 2
        fi 
        if [ "`which tar`" = "" ]; then
            sudo yum -y install tar
        fi
        if [ "`which sed`" = "" ]; then
            sudo yum -y install sed 
        fi
        if [ "`which awk`" = "" ]; then
            sudo yum -y install awk
        fi
        if [ "`which cmake`" = "" ]; then
            sudo yum -y install cmake 
        fi
        if [ "`which wget`" = "" ]; then
            sudo yum -y install wget
        fi
    fi 
fi

# Get ProductKey, DeviceName, DeviceSecret and region
jsonfile=device_id_password.json

if [ ! -f "./${jsonfile}" ]; then
    echo "Error: json file is not exist"
    exit 3
fi

pk=`grep -Po '(?<=productKey": ")[0-9a-zA-Z]*' ${jsonfile}`
dn=`grep -Po '(?<=deviceName": ")[\-_@\.:0-9a-zA-Z]*' ${jsonfile}`
ds=`grep -Po '(?<=deviceSecret": ")[0-9a-zA-Z]*' ${jsonfile}`
region=`grep -Po '(?<=region": ")[\-_\.:0-9a-zA-Z]*' ${jsonfile}`
mqttdomain="iot-as-mqtt."${region}".aliyuncs.com"
httpdomain="iot-auth."${region}".aliyuncs.com"
endpoint=`grep -Po '(?<=endpoint": ")[\-_\.:0-9a-zA-Z]*' ${jsonfile}`

# Download iotx-sdk-c if itn't exist
url=https://github.com/aliyun/iotkit-embedded/archive/v2.3.0.zip
sdkzip=iotkit-embedded-2.3.0.zip
sdkdir=iotkit-embedded-2.3.0

if [ ! -d "./${sdkdir}" ]; then
    echo "sdk is not exist"
    if [ ! -f "/${sdkzip}" ]; then
        echo "downloading sdk zip from github"
        wget -O ${sdkzip} ${url}
        unzip ${sdkzip}
        rm -f ${sdkzip}
    else
        unzip ${sdkzip}
        rm -f ${sdkzip}
    fi
fi

# compile the sdk and copy /lib, /include to quickstart dir
if [ ! -d "./lib" ] || [ ! -d "./include" ]; then
    cd ${sdkdir}
    make distclean
    make
    cd ..
    cp -r ./${sdkdir}/output/release/lib ./lib
    cp -r ./${sdkdir}/output/release/include ./include    
fi

if [ ! -n "$endpoint" ]; then
echo "null"
endpoint=NULL
fi

# compile the sample and run
make clean -s
make all PK=${pk} DN=${dn} DS=${ds} DOMAIN=${mqttdomain} ENDPOINT=${endpoint}
./quickstart


