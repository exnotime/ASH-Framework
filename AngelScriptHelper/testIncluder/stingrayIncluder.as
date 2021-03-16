string CoreFolder = "";

bool InitializeIncluder(){
    //need to map core folder
    string srbinDir = GetEnvVar("SR_BIN_DIR");
    string toolchainPath = srbinDir + "/settings/ToolChainConfiguration.config";
    Config toolchain = Config();
    if(!toolchain.parse_file(toolchainPath)){
        return false;
    }
    Config@ coreDcv = toolchain["SourceRepositoryPath"];
    if(coreDcv.is_string()){
        CoreFolder = coreDcv.to_string();
    }else{
        CoreFolder = srbinDir + "/core/";
    }
    print("'Core' mapped to: " + CoreFolder);
    return true;
}

string IncludeFile(string includeFile, string fromFile){
    int lastSlash = fromFile.findLast("/");
    string dir = fromFile.substr(0, lastSlash + 1);
    if(includeFile.substr(0, 5) == "core/"){
        includeFile = CoreFolder + includeFile;
    }
    file f;
    print("Including: " + includeFile);
    if(f.open(dir + includeFile, "r") >= 0 ){
        string s = f.readString(f.getSize());
        f.close();
        return s;
    }

    return "error";
}