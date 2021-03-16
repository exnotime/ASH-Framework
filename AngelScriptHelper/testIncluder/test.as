string IncludeFile(string includeFile, string fromFile){
    int lastSlash = fromFile.findLast("/");
    string dir = fromFile.substr(0, lastSlash + 1);
    file f;
    if(f.open(dir + includeFile, "r") >= 0 ){
        string s = f.readString(f.getSize());
        f.close();
        return s;
    }

    return "error";
}