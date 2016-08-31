#!/bin/bash
function usage {
    printf "Usage: \n"
    printf "    build {platform}, build for platforms (ios/iphoneos, macosx/macos)\n"
    printf "    package, add to build command to build package (only for ios/iphoneos platforms)\n"
    printf "    install {IP} {PORT} {USER}, install to ip and port, if ip isn't provided, THEOS_DEVICE_IP is used, and if port isn't provided THEOS_DEVICE_PORT is used, THEOS_DEVICE_USER is used if USER isn't provided\n"

    exit 0
}

function error {
    printf "\x1B[31mError:\x1B[0m $1\n"
    exit -1
}

if [ $# -eq 0 ]; then
    usage
fi

if [ "$1" = "build" ]; then
    if [ $# -lt 2 ]; then
        error "Please provide a platform (ios/iphoneos or macos/macosx)"
    fi

    #deleting build/ fixes a linker error?
    rm -rf build

    platform=$2
    if [ "$platform" = "ios" ] || [ "$platform" = "iphoneos" ]; then
        cmake . -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=iphoneos.cmake -DIOS_PLATFORM=OS -DIOS_DEPLOYMENT_TARGET=9.0 -B./build
    elif [ "$platform" = "macos" ] || [ "$platform" = "macosx" ]; then
        cmake . -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=macosx.cmake -DMACOS_DEPLOYMENT_TARGET=10.8 -B./build
    else
        error "Invalid option \"$platform\""
    fi

    make -C build
    ldid -S build/rmaslr

    if [ $# -gt 2 ]; then
        if [ "$3" = "package" ]; then
            mkdir _temp_package
            cd _temp_package

            mkdir DEBIAN
            cp ../control DEBIAN/

            mkdir usr
            cd usr

            mkdir bin
            cp ../../build/rmaslr bin/

            cd ../../

            output=$(dpkg-deb -b _temp_package 1>&1)

            package_id=`dpkg-deb -f _temp_package.deb Package`
            version=`dpkg-deb -f _temp_package.deb Version`
            architecture=`dpkg-deb -f _temp_package.deb Architecture`

            rm -rf _temp_package.deb

            if [ ! -d "packages" ]; then
                mkdir packages
            fi

            cd packages/
            package_name=$package_id"_"$version"_"$architecture

            if [ -e "$package_name.deb" ]; then
                rm "$package_name.deb"
            fi

            cd ..
            mv _temp_package "packages/$package_name"

            cd packages
            dpkg-deb -Zgzip -b "$package_name"

            rm -rf "$package_name"

            cd ..
            rm -rf _temp_package

            if [ $# -gt 3 ]; then
                if [ "$4" = "install" ]; then
                    cd packages

                    ip=""
                    port=""
                    user=""

                    if [ $# -gt 4 ]; then
                        ip=$5
                        if [ $# -gt 5]; then
                            port=$6
                            if [ $# -gt 6]; then
                                user=$7
                                if [ $# -gt 7]; then
                                    error "Too many arguments provided"
                                fi
                            else
                                user=$THEOS_DEVICE_USER
                            fi
                        else
                            port=$THEOS_DEVICE_PORT
                            user=$THEOS_DEVICE_USER
                        fi
                    else
                        ip=$THEOS_DEVICE_IP
                        port=$THEOS_DEVICE_PORT
                        user=$THEOS_DEVICE_USER
                    fi

                    scp -P $port "./$package_name.deb" root@$ip:/User/
                    ssh -p $port -l root $ip " dpkg -i /User/$package_name.deb && rm /User/$package_name.deb "
                fi
            fi
        elif [ "$3" = "install" ]; then
            ip=""
            port=""
            user=""

            if [ $# -gt 4 ]; then
                ip=$5
                if [ $# -gt 5]; then
                    port=$6
                    if [ $# -gt 6]; then
                        user=$7
                        if [ $# -gt 7]; then
                            error "Too many arguments provided"
                        fi
                    else
                        user=$THEOS_DEVICE_USER
                    fi
                else
                    port=$THEOS_DEVICE_PORT
                    user=$THEOS_DEVICE_USER
                fi
            else
                ip=$THEOS_DEVICE_IP
                port=$THEOS_DEVICE_PORT
                user=$THEOS_DEVICE_USER
            fi

            scp -P $port "build/rmaslr" root@$ip:/usr/bin/
        else
            error "Invalid Option \"$3\""
        fi
    fi
else
    error "Invalid option \"$1\""
fi
