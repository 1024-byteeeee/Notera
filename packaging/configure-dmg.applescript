tell application "Finder"
    tell disk "Notera"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set bounds of container window to {120, 120, 840, 560}
        set theViewOptions to icon view options of container window
        set arrangement of theViewOptions to not arranged
        set icon size of theViewOptions to 112
        set background picture of theViewOptions to file ".background:background.png"
        set position of item "Notera.app" of container window to {190, 220}
        set position of item "Applications" of container window to {530, 220}
        update without registering applications
        delay 2
        close
    end tell
end tell
