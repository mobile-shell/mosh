describe() {
    /opt/local/bin/port version > port-version.txt
    /opt/local/bin/port -v -q installed > port-packages.txt
}

deps() {
    # Set up macports to respect the deployment target before building universal packages
    echo "macosx_deployment_target 11.0" | sudo tee -a /opt/local/etc/macports/macports.conf
    sudo /opt/local/bin/port install protobuf3-cpp +universal
    sudo /opt/local/bin/port install ncurses +universal
    sudo /opt/local/bin/port install pkgconfig
    sudo /opt/local/bin/port install autoconf automake
}

set -e
"$@"
