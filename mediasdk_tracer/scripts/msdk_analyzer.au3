Global $dfile
Global $WinTitle
Global $LogFile
Global $ConfigFile
Global $sleep
Global $click

Start()

Func Start()
	
	ProcessCommandLine()
	ExecuteScenario()
	
EndFunc

Func ExecuteScenario()
    $ConfigFH = FileOpen($ConfigFile, 0)
    If ($ConfigFH = -1) Then
        MsgBox(0, "Error!", "Can't open config file " & $ConfigFile)
        Exit (2)
    EndIf
 
    While 1
        $SearchLine = FileReadLine($ConfigFH)
        If (@error = -1) Then
            ExitLoop 1
        EndIf
        $SearchLine = StringStripWS($SearchLine, 7)
        If ($SearchLine = "" Or StringLeft($SearchLine, 1) = ";") Then
            ContinueLoop
        EndIf
        $field = StringSplit($SearchLine, " ")
        If ($field[0] < 2) Then
            DebugFileWriteLine("!!! Incorrect configuration file line '" & $SearchLine & "'")
        Else
            If ($field[0] > 2) Then
                For $i = 3 to $field[0] Step 1
                    $field[2] = $field[2] & " " & $field[$i]
                Next
            EndIf
            $val = StringReplace(StringStripWS($field[2], 7), """", "")
            Select
                Case (StringStripWS($field[1], 7) = "sleep")
                    DebugFileWriteLine("Sleep " & $val & " sec")
                    Sleep ($val*1000)
                Case (StringStripWS($field[1], 7) = "click")
                    ActivateWindow()
                    Click($val)
                Case (StringStripWS($field[1], 7) = "gettext")
                    ActivateWindow()
                    GetText($val)
                Case (StringStripWS($field[1], 7) = "settext")
                    $field = StringSplit($val, "=")
                    ActivateWindow()
                    SetText($field[1], $field[2])
            EndSelect
        EndIf
    WEnd
    FileClose($ConfigFH)	
EndFunc

Func ProcessCommandLine()
   Local $i

	If UBound($CmdLine) = 1 Then
		MsgBox(1,"Usage", "Command line parameters:" & Chr(10) & "-l logfile" & Chr(10) & "-c config_file" & Chr(10) & "-t window_title")
		Exit
	EndIF

	For $i = 1 to $CmdLine[0]
		If ($CmdLine[$i] = "-h" Or $CmdLine[$i] = "-help") Then
			$help = 1
		ElseIf ($CmdLine[$i] = "-t" Or $CmdLine[$i] = "-title") Then
			If ($i < $CmdLine[0]) Then
				$WinTitle = $CmdLine[$i+1]
				DebugFileWriteLine("$WinTitle = " & $WinTitle)
				$i = $i + 1
			EndIf
		ElseIf ($CmdLine[$i] = "-c" Or $CmdLine[$i] = "-config") Then
			If ($i < $CmdLine[0]) Then
				$ConfigFile = $CmdLine[$i+1]
				DebugFileWriteLine("$ConfigFile = " & $ConfigFile)
				$i = $i + 1
			EndIf
		ElseIf ($CmdLine[$i] = "-l" Or $CmdLine[$i] = "-log") Then
			If ($i < $CmdLine[0]) Then
				$LogFile = $CmdLine[$i+1]
				DebugFileWriteLine("$LogFile = " & $LogFile)
				$i = $i + 1
			EndIf		   
		Else
			MsgBox(48, "SN Checker Error", "Error: Unrecognized command-line option " & $CmdLine[$i])
		EndIf
	Next
EndFunc

Func DebugFileWriteLine($text)
	If $dfile == "" And $LogFile <> "" Then
		DebugInit($LogFile, 1)
	EndIf    
	If $dfile <> "" Then
		If $text <> "" Then
			$msg = StringFormat("%s/%s/%s %s:%s:%s", @MON, @MDAY, @YEAR, @HOUR, @MIN, @SEC)
			FileWriteLine($dfile, $msg & " " & $text)
		Else
			FileWriteLine($dfile, $text)
		EndIf
	EndIf
EndFunc

Func DebugInit($filename, $mode)
	If $dfile == "" Then
		If $filename == "" Then
			$filename = "debug.txt"
		EndIf
		$dfile = FileOpen($filename, $mode)
		If ($dfile = -1) Then
			MsgBox(0, "Error", "Can not open " & $filename & " for writing")
		Exit(1)
		EndIf
		FileWriteLine($dfile, "")
	EndIf
EndFunc

Func ActivateWindow()
    $Error = 0
    If WinWait($WinTitle, "", 10) Then
        DebugFileWriteLine("WinActivate Title: " & $WinTitle)
        WinActivate($WinTitle, "")
        $Error = 0
    Else
        $Error = 1
    EndIf
    If $Error == 1 Then
        PV_DebugFileWriteLine("Error: WinActivate failed")
        return
    EndIf      
EndFunc

Func Click($name)
    DebugFileWriteLine("ControlClick Title = " & $WinTitle & ", controlID = " & $name)
    $error = ControlClick( $WinTitle, "", $name)
    If $error == 0 Then 
        DebugFileWriteLine("Error: ControlClick failed")
    EndIf
EndFunc

Func SetText($name, $text)
    DebugFileWriteLine("ControlSetText Title = " & $WinTitle & ", controlID = " & $name & ", Text = " & $text)
    $error = ControlSetText ( $WinTitle, "", $name, $text)
    If $error == 0 Then 
        DebugFileWriteLine("Error: ControlClick failed")
    EndIf
EndFunc

Func GetText($name)
    DebugFileWriteLine("ControlSetText Title = " & $WinTitle & ", controlID = " & $name)
    $text = ControlGetText ( $WinTitle, "", $name)
    ConsoleWrite($name & " " & $text & @CRLF)
EndFunc