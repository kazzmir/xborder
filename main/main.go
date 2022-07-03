package main

import (
    "github.com/BurntSushi/xgb/xproto"
    "github.com/BurntSushi/xgbutil"
    "github.com/BurntSushi/xgbutil/xwindow"
    "log"
)

func main(){
    log.SetFlags(log.Ldate | log.Lshortfile | log.Lmicroseconds)
    X, err := xgbutil.NewConn()
    if err != nil {
        log.Printf("Could not connect to X server: %v", err)
        return
    }
    defer X.Conn().Close()
    log.Printf("Connected to X\n")

    root := xwindow.New(X, X.RootWin())
    tree, err := xproto.QueryTree(X.Conn(), root.Id).Reply()
    if err != nil {
        log.Printf("Could not query tree: %v", err)
        return
    }

    for _, window := range tree.Children {
        log.Printf("Window: %v", window)
    }

    log.Printf("Shutting down")
}
