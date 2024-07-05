on run argv
  set image_name to item 1 of argv

  tell application "Finder"
  tell disk image_name

    set open_attempts to 0
    repeat while open_attempts < 4
      try
        open
          delay 1
          set open_attempts to 5
        close
      on error errStr number errorNumber
        set open_attempts to open_attempts + 1
        delay 10
      end try
    end repeat
    delay 5

    open
      set current view of container window to icon view
      set theViewOptions to the icon view options of container window
      set background picture of theViewOptions to file ".background:background.png"
      set arrangement of theViewOptions to not arranged
      set icon size of theViewOptions to 64
      delay 5
    close
    
    open
      update without registering applications
      tell container window
        set sidebar width to 0
        set statusbar visible to false
        set toolbar visible to false
        set the bounds to { 400, 100, 1001, 502 }
		set position of item "hyperhdr.app" to { 200, 222 }
		set position of item "Applications" to { 400, 222 }
		set position of item ".fseventsd" to { 1000, 222 }
		set position of item ".background" to { 1000, 222 }
      end tell
      update without registering applications
      delay 5
    close

    open
      delay 5
    close

  end tell
  delay 1
end tell
end run