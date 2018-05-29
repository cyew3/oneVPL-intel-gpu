function ParseVcxproj($filename)
{
echo "FullPath	$filename"

[xml]$doc = Get-Content $filename

foreach($item in $doc.Project.Import)
{
    echo "Import	$($item.Project)"
}

foreach($item in $doc.Project.PropertyGroup)
{
    if ($item.ProjectGuid -ne $null)
    {
        echo "GUID	$($item.ProjectGuid)"
    }
    if ($item.RootNamespace -ne $null)
    {
        echo "RootNamespace	$($item.RootNamespace)"
    }
    if($item.TargetName -ne $null)
    {
        foreach($target in $item.TargetName)
        {
            echo "TargetName	$($target.Condition)	$($target.InnerText)"
        }
    }
}

foreach($item in $doc.Project.ItemDefinitionGroup)
{
    echo "Configuration	$($item.Condition)"
    if ($item.ClCompile -ne $null)
    {
        foreach($opt in $item.ClCompile.ChildNodes)
        {
            echo "$($opt.ToString())	$($opt.InnerText)"
        }
    }
    if ($item.Link -ne $null)
    {
        foreach($opt in $item.Link.ChildNodes)
        {
            echo "$($opt.ToString())	$($opt.InnerText)"
        }
    }
    echo "End Of Configuration"
}
}

ls -Recurse -Filter "*.vcxproj" | % {
    ParseVcxproj($_.FullName)
}

