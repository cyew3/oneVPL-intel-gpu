function CopyAttributes
{
    foreach($attr in $from.Attributes)
    {
        [string]$name = $attr.ToString()
        $to.SetAttribute($name, $from.GetAttribute($name))
    }
}

function AddSmartImport($name)
{
    $node = $new.CreateElement("Import")
    $node.SetAttribute("Condition", "Exists('`$(SolutionDir)\Intel.Mfx.$name.props') and '`$(Mfx$($name)Included)'==''")
    $node.SetAttribute("Project", "`$(SolutionDir)\Intel.Mfx.$name.props")
    $new.Project.AppendChild($node)
    $node = $new.CreateElement("Import")
    $node.SetAttribute("Condition", "Exists('`$(MEDIASDK_LIB_ROOT)\builder\Intel.Mfx.$name.props') and '`$(Mfx$($name)Included)'==''")
    $node.SetAttribute("Project", "`$(MEDIASDK_LIB_ROOT)\builder\Intel.Mfx.$name.props")
    $new.Project.AppendChild($node)
    $node = $new.CreateElement("Import")
    $node.SetAttribute("Condition", "Exists('`$(MSBuildThisFileDirectory)$relPath\Intel.Mfx.$name.props') and '`$(Mfx$($name)Included)'==''")
    $node.SetAttribute("Project", "`$(MSBuildThisFileDirectory)$relPath\Intel.Mfx.$name.props")
    $new.Project.AppendChild($node)
}


copy-item $args "$args.bak"

[xml]$doc = Get-Content $args
[xml]$new = new-object System.Xml.XmlDocument

$node = $new.AppendChild($new.CreateElement("Project"))
[xml.xmlelement]$from = $doc.DocumentElement
[xml.xmlelement]$to = $new.DocumentElement
CopyAttributes

$tmp = Get-Location
$root = Split-Path $args
Set-Location $root
$relPath = Resolve-Path -Relative $ENV:MEDIASDK_ROOT\mdp_msdk-lib\builder
Set-Location $tmp

$propsFileName = [System.IO.Path]::GetFileNameWithoutExtension($args)
$propsFilePath = "$root\$propsFileName.props"

$projVCName = "<!-- $([System.IO.Path]::GetFileName($args)) -->";

foreach($item in $doc.Project.PropertyGroup)
{
    if($item.Label -eq "Globals" )
    {
        AddSmartImport "Header"
    
        $node = $new.ImportNode($item, $true)
        $node.SetAttribute("xmlns", "")
        foreach($rNode in $node.ChildNodes)
        {
          if($rNode -ne $null -and $rNode.ToString() -eq "WindowsTargetPlatformVersion")
          {
            $node.RemoveChild($rNode);
          }
          if($rNode -ne $null -and $rNode.ToString() -eq "ProjectGuid")
          {
            $projGuid = "<!-- $($rNode.InnerText) -->";
          }
          if($rNode -ne $null -and $rNode.ToString() -eq "ProjectName")
          {
            $projName = "<!-- $($rNode.InnerText) -->";
          }
        }
        $nNode = $new.CreateElement("WindowsTargetPlatformVersion")
        $nNode.InnerText = "`$(MfxWindowsTargetPlatformVersion)"
        $node.AppendChild($nNode)
        $new.Project.AppendChild($node)

        $node = $new.CreateElement("Import")
        $node.SetAttribute("Project", "`$(VCTargetsPath)\Microsoft.Cpp.Default.props")
        $new.Project.AppendChild($node)

        $node = $new.CreateElement("Import")
        $node.SetAttribute("Project", "`$(MSBuildThisFileDirectory)$propsFileName.props")
        $new.Project.AppendChild($node)

        AddSmartImport "Default"

        $node = $new.CreateElement("PropertyGroup")
        $node.SetAttribute("Label", "Configuration")
        $nNode = $new.CreateElement("PlatformToolset")
        $nNode.InnerText = "`$(MfxPlatformToolset)"
        $node.AppendChild($nNode)
        $new.Project.AppendChild($node)

        $node = $new.CreateElement("Import")
        $node.SetAttribute("Project", "`$(VCTargetsPath)\Microsoft.Cpp.props")
        $new.Project.AppendChild($node)

        AddSmartImport "Building"
    }
}

#[xml.xmlelement]$ClCompile = $new.CreateElement("ItemGroup")
#[xml.xmlelement]$ClInclude = $new.CreateElement("ItemGroup")
#$new.Project.AppendChild($ClCompile)
#$new.Project.AppendChild($ClInclude)

foreach($item in $doc.Project.ItemGroup)
{
    if($item.Label -eq $null -or $item.Label -ne "ProjectConfigurations")
    {
            $node = $new.ImportNode($item, $true)
            $node.SetAttribute("xmlns", "")
            $new.Project.AppendChild($node)
    }
#
#    foreach($sitem in $item.ChildNodes)
#    {
#        [string]$name = $sitem.ToString()
#        if($name -eq "ClCompile")
#        {
#            $node = $new.ImportNode($sitem, $true)
#            $node.SetAttribute("xmlns", "")
#            $ClCompile.AppendChild($node)
#        }
#        if($name -eq "ClInclude")
#        {
#            $node = $new.ImportNode($sitem, $true)
#            $node.SetAttribute("xmlns", "")
#            $ClInclude.AppendChild($node)
#        }
#    }
}

$node = $new.CreateElement("Import")
$node.SetAttribute("Project", "`$(VCTargetsPath)\Microsoft.Cpp.targets")
$new.Project.AppendChild($node)

AddSmartImport "Footer"


$new = [xml] $new.OuterXml.Replace(" xmlns=`"`"", "")
$new.Save((Resolve-Path $args))

$propsHeader = "<?xml version=`"1.0`" encoding=`"utf-8`"?>
<Project ToolsVersion=`"4.0`" xmlns=`"http://schemas.microsoft.com/developer/msbuild/2003`">
</Project>"
$props = "$projVCName
$projGuid
$projName
"
if ( $(Get-Item $propsFilePath).Length -le 0kb )
{
#$propsHeader | Out-File -FilePath $propsFilePath -Append
}

#$props | Out-File -FilePath $propsFilePath -Append
