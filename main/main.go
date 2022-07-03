package main

import (
    "github.com/BurntSushi/xgb/xproto"
    "github.com/BurntSushi/xgbutil"
    "github.com/BurntSushi/xgbutil/xwindow"
    "github.com/BurntSushi/xgbutil/xevent"
    "github.com/BurntSushi/xgbutil/mousebind"
    "log"
    "time"
)

func chooseWindow(X *xgbutil.XUtil, root *xwindow.Window) xproto.Window {

    log.Printf("Click on a window")
    pressed := make(chan xproto.Window, 1)
    err := mousebind.ButtonPressFun(
        func (X *xgbutil.XUtil, event xevent.ButtonPressEvent){
            log.Printf("Button pressed on window root=0x%x event=0x%x child=0x%x", event.Root, event.Event, event.Child)
            /*
            err := xproto.AllowEventsChecked(X.Conn(), xproto.AllowReplayPointer, xproto.TimeCurrentTime).Check()
            if err != nil {
                log.Printf("Could not allow events: %v", err)
            }
            */

            pressed <- event.Child
    }).Connect(X, root.Id, "1", true, true)

    if err != nil {
        log.Printf("Could not grab: %v", err)
        return root.Id
    }

    defer mousebind.DetachPress(X, root.Id)

    select {
        case window := <-pressed:
            log.Printf("Pressed a mouse button")
            return window
        case <-time.After(5 * time.Second):
            log.Printf("Timed out")
    }

    return root.Id
}

func main(){
    log.SetFlags(log.Ldate | log.Lshortfile | log.Lmicroseconds)
    X, err := xgbutil.NewConn()
    if err != nil {
        log.Printf("Could not connect to X server: %v", err)
        return
    }
    defer X.Conn().Close()
    log.Printf("Connected to X\n")

    mousebind.Initialize(X)

    root := xwindow.New(X, X.RootWin())
    tree, err := xproto.QueryTree(X.Conn(), root.Id).Reply()
    if err != nil {
        log.Printf("Could not query tree: %v", err)
        return
    }

    /* process X events */
    go xevent.Main(X)

    for _, window := range tree.Children {
        log.Printf("Window: %v", window)
    }

    window := chooseWindow(X, root)
    log.Printf("Chose window %v", window)

    log.Printf("Shutting down")
}
