#include <sys/types.h>
#include <sys/ioctl.h> 
#include <sys/stat.h>
#include <sys/wait.h> 
#include <algorithm>
#include <termios.h>
#include <dirent.h>
#include <signal.h>
#include <iostream>
#include <unistd.h> 
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <string>
#include <queue>
#include <vector>
#include <time.h>
#include <stack>
#include <pwd.h>
#include <grp.h>
using namespace std;

#define ESC 27
#define K_UP 65
#define K_DOWN 66
#define K_LEFT 68
#define K_RIGHT 67
#define LEAVE_LINES 3   //Last 3 lines should be empty
int col_position=0, owner_col_no=1, group_col_no=2, size_col_no=3, start_index=-1, end_index=-1, sizelen=0, ownerlen=0, grouplen=0;
int row_cursor=1, col_cursor=1, d_output_index=0; //index for row_cursor and col_cursor starts from 1 
char cwd[100];  //maximum current working directory length is fixed to 99 characters.
stack<string> back_stack, forward_stack;
vector<vector<string>> d_output;
struct winsize size;

bool comparator(vector<string>, vector<string>);
void set_position_value(int &, int &, int &);
string get_full_file_path(string, string);
string execute_command(vector<string>&);
void initialise_display_command_mode();
string get_full_directory_path(string);
void get_window_size(struct winsize&); 
string move_directory(string, string);
string copy_directory(string, string);
string get_permissions(struct stat&);
mode_t convert_permissions(string&);
int rename_file(string &, string &);
string copy_file(string&, string&);
int create_file(string &, mode_t);
int create_dir(string &, mode_t);
string remove_directory(string);
void position_cursor(int, int);
bool search(string, string);
bool is_directory(string);
void print(stack<string>);
string get_parent(string);
int delete_file(string&);
int delete_dir(string &);
int nmode_keyinput();
int cmode_keyinput();
int get_line_count();
int list_out(string);
void clearscreen();
void display();

void initialise_display_command_mode() {
    //display colon in 2nd last line
    position_cursor(size.ws_row-1,1);
    printf(": ");

    //clear last line
    position_cursor(size.ws_row,1);
    printf("%c[K",ESC);
    
    //position cursor with one space next to colon
    position_cursor(size.ws_row-1,3);
}

void clearscreen() {
    printf("%c[2J%c[1;1H",ESC,ESC);
}

int get_line_count() {
    return d_output.size();
}

void position_cursor(int x, int y) {
    printf("%c[%d;%dH",ESC,x,y);
}

void get_window_size(struct winsize &size) {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
}

void set_position_value(int &xpos, int &ypos, int &index) {
    //set initial cursor position variables
    xpos = get_line_count();
    index = end_index;
    if(xpos>size.ws_row) {
        xpos = size.ws_row-LEAVE_LINES;   
    }
    ypos = col_position;
}

void display() {
    string temp;
    int index;

    clearscreen();
    get_window_size(size);
    
    if(start_index==-1 && end_index==-1) {  
        //set initial values
        start_index=0;
        end_index=get_line_count()-1;

        //leaving LEAVE_LINES lines at the bottom of screen
        if((end_index-start_index)>(size.ws_row-LEAVE_LINES-1)) { 
            end_index = size.ws_row-LEAVE_LINES-1;
        } 
    } else if(start_index<0)  { 
        //edge cases for scrolling
        ++start_index;
        ++end_index;
    } else if(end_index>=get_line_count()) {
        //edge cases for scrolling
        --start_index;
        --end_index;
    }

    for(int row=start_index; row<=end_index; ++row) {
        for(int col=0; col<d_output[row].size(); ++col) {
            if(col==size_col_no) {
                printf("%*s ",sizelen, d_output[row][col].c_str());
            } else  if(col==owner_col_no) {
                printf("%*s ",ownerlen, d_output[row][col].c_str());
            } else  if(col==group_col_no) {
                printf("%*s ",grouplen, d_output[row][col].c_str());
            } else if(col==(d_output[row].size()-1)) {
                index=d_output[row][col].find_last_of("/")+1;
                temp = d_output[row][col].substr(index);
                printf("%s ",temp.c_str());
            } else {
                printf("%s ",d_output[row][col].c_str());
            }
        }
        cout<<endl;
    }

    //Show status bar at bottom of screen
    position_cursor(size.ws_row,1);
    cout<<"NORMAL MODE";
}

void print(stack<string> st) {
    stack<string> temp = st;
    cout<<endl<<endl;
    while(!st.empty()) {
        cout<<st.top()<<endl;
        st.pop();
    }
}

void clear_stack(stack<string> &st) {
    while(!st.empty()) {
        st.pop();
    }
}

bool is_directory(string path) {
    bool res=false;
    struct stat d_info;
    stat(path.c_str(),&d_info);
    if(S_ISDIR(d_info.st_mode)) {
        res=true;
    }
    return res;
}

string get_full_file_path(string path, string file) {
    string res=path;

    if(res[res.length()-1]=='/') {
        res.resize(res.length()-1);
    }

    if(file.length()>1 && (file[0]=='~' || (file[0]=='.' && file[1]=='/') )) {
        file=file.substr(2);
        res=res+'/'+file;
    } else if(file[0]!='~' && file[0]!='.') {
        res=res+'/'+file;
    } else if(file[0]=='.' && file.length()>1) {
        res=res+'/'+file;
    }

    return res;
}

string get_full_directory_path(string dir) {
    string res=cwd;

    if(dir.length()>1 && (dir[0]=='~' || (dir[0]=='.' && dir[1]=='/') )) {
        dir=dir.substr(2);
        res=res+'/'+dir;
    } else if(dir[0]!='~' && dir[0]!='.') {
        res=res+'/'+dir;
    } else if(dir[0]=='.' && dir.length()>1) {
        res=res+'/'+dir;
    }
    
    return res;
}

string execute_command(vector<string> &command) {
    string err_msg="", permissions, temp, temp2, dir_path;
    queue<string> dir_q;
    struct stat d_info;
    int index;

    if(command[0]=="copy") {
        if(command.size()<3) {
            err_msg="Invalid number of arguments";
        } else {

            for(int i=1; i<command.size()-1; ++i) {
                dir_path = get_full_directory_path(command[command.size()-1]);                
                temp = get_full_file_path(cwd, command[i]);

                if(is_directory(temp)) {
                    dir_q.push(temp);
                } else {
                    err_msg = copy_file(temp,dir_path);
                    if(err_msg.size()) {
                        break;
                    }
                }
            } 
            while(!(dir_q.empty())) {
                err_msg=copy_directory(dir_q.front(),dir_path);
                dir_q.pop();
                if(err_msg.length()) {
                    break;
                }
            }
        }
    } else if(command[0]=="move") {
        if(command.size()<3) {
            err_msg = "Invalid number of arguments";
        } else {
            for(int i=1; i<command.size()-1; ++i) {
                dir_path = get_full_directory_path(command[command.size()-1]);
                temp = get_full_file_path(cwd, command[i]);

                if(is_directory(temp)) {
                    dir_q.push(temp);
                } else {
                    err_msg = copy_file(temp,dir_path);
                    if(err_msg.size()) {
                        break;
                    }
                    //delete file
                    if(delete_file(temp)!=0) {
                        err_msg = "Error deleting file";
                        break;
                    }
                }
            } 
            while(!(dir_q.empty())) {
                err_msg=move_directory(dir_q.front(),dir_path);
                dir_q.pop();
                if(err_msg.length()) {
                    break;
                }
            }
        }
    } else if(command[0]=="rename") {
        if(command.size()<3) {
            err_msg = "Invalid number of arguments";
        } else {
            temp = get_full_file_path(cwd, command[1]);
            temp2 = get_full_file_path(cwd, command[2]);
            if(rename_file(temp,temp2)!=0) {
                err_msg = "Error renaming file";
            }
        }
    } else if(command[0]=="create_file") {  
        //1st argument should be just the file name
        if(command.size()!=3) {
            err_msg = "Invalid number of arguments";
        } else {
            dir_path = get_full_directory_path(command[command.size()-1]);
            temp = get_full_file_path(dir_path, command[1]);
            stat(dir_path.c_str(),&d_info);
            permissions = get_permissions(d_info);
            if(create_file(temp,convert_permissions(permissions))==-1) {
                err_msg="Error creating file";
            }
        }
    } else if(command[0]=="create_dir") {   
        if(command.size()!=3) {
            err_msg = "Invalid number of arguments";
        } else {
            dir_path = get_full_directory_path(command[command.size()-1]);
            stat(dir_path.c_str(),&d_info);
            permissions = get_permissions(d_info);
            temp = get_full_file_path(cwd,command[1]);

            index = temp.find_last_of('/');
            if(index==string::npos) {
                err_msg="Full path not specified";
            } else {
                if(index!=0 && temp.length()>1) { 
                    //not root directory
                    dir_path = dir_path+"/"+temp.substr(index+1);
                } else {
                    dir_path = dir_path+"/";
                }
                if(create_dir(dir_path,convert_permissions(permissions))==-1) {
                    err_msg="Error creating directory";
                }
            }
        }
    } else if(command[0]=="delete_file") {  
        //file path is relative to cwd. No need to convert to full path
        if(command.size()!=2) {
            err_msg = "Invalid number of arguments";
        } else {    
            //check if / exists at end
            if(command[1][command[1].length()-1]=='/') {
                err_msg="Invalid file name";
            } else {
                temp = get_full_file_path(cwd, command[1]);
                if(delete_file(temp)==-1) {
                    err_msg="Error deleting file";
                }
            }            
        }
    } else if(command[0]=="delete_dir") {
        if(command.size()!=2) {
            err_msg = "Invalid number of arguments";
        } else {            
            temp = get_full_file_path(cwd, command[1]);

            if(is_directory(temp)) {
                dir_q.push(temp);
            } else {
                err_msg = "You have entered file";
            }
            while(!(dir_q.empty())) {
                err_msg=remove_directory(dir_q.front());
                dir_q.pop();
                if(err_msg.length()) {
                    break;
                }
            }
        }
    } else if(command[0]=="goto") {
        if(command.size()!=2) {
            err_msg = "Invalid number of arguments";
        } else {
            temp = get_full_directory_path(command[1]);

            if(temp[temp.length()-1]=='.' && temp[temp.length()-2]=='.') {
                temp = get_parent(temp);
            }

            if(is_directory(temp)) {
                start_index=end_index=-1;
                int err = list_out(temp);
                if(err) {
                    err_msg = "Error in displaying content of directory";
                }
                set_position_value(row_cursor, col_cursor, d_output_index);
                position_cursor(row_cursor, col_cursor);
                back_stack.push(temp);
                initialise_display_command_mode();
            } else {
                err_msg = "You have entered file";
            }
        }
    } else if(command[0]=="search") {
        if(command.size()!=2) {
            err_msg = "Invalid number of arguments";
        } else {
            temp = get_full_file_path(back_stack.top(),command[1]);
            position_cursor(size.ws_row,1);
            if(search(temp,back_stack.top())) {
                cout<<"True";
            } else {
                cout<<"False";
            }
        }
    } else {
        err_msg="Invalid command";
    }
    
    return err_msg;
}

bool search(string file, string dest) {
    string err_msg="", dir_name, temp, filename="";
    struct dirent *d_entry;
    queue<string> dirs;
    struct stat d_info;
    bool found=false;
    int index;
    DIR *dir;

    index = file.find_last_of('/');
    if(index==string::npos) {
        return found;
    } else {
        if(file.length()>1) {
            filename = file.substr(index+1);
        } else {
            // if file is just /
            return found;
        }
    }

    dirs.push(dest);
    while(!(dirs.empty())) {
        dir_name = dirs.front();
        dirs.pop();

        dir = opendir(dir_name.c_str());
        if(dir==NULL) {
            err_msg="Could not open current directory";
            break;
        }
        d_entry = readdir(dir);
        while(d_entry!=NULL) {
            temp = d_entry->d_name;
            if(temp!="." && temp!="..") {
                temp = dir_name+'/'+d_entry->d_name;
                if(filename==d_entry->d_name) {
                    found=true;
                    break;
                } else if(is_directory(temp)) {
                    dirs.push(temp);
                }
            } 
            d_entry = readdir(dir);
        } 
        if(found) {
            break;
        }
        closedir(dir);
    }
    return found;
}

string remove_directory(string dir_og) {
    string err_msg="", dir_name, temp;
    struct dirent *d_entry;
    stack<string> dirs_rm;
    queue<string> dirs;
    struct stat d_info;
    DIR *dir;

    dirs.push(dir_og);
    while(!(dirs.empty())) {
        dir_name = dirs.front();
        dirs_rm.push(dir_name);
        dirs.pop();

        dir = opendir(dir_name.c_str());
        if(dir==NULL) {
            err_msg="Could not open current directory";
            break;
        }
        d_entry = readdir(dir);
        while(d_entry!=NULL) {
            temp = d_entry->d_name;
            if(temp!="." && temp!="..") {
                temp = dir_name+'/'+d_entry->d_name;
                stat(temp.c_str(), &d_info);

                //check file type
                if(S_ISREG(d_info.st_mode)) {
                    delete_file(temp);
                } else if(S_ISDIR(d_info.st_mode)) {
                    dirs.push(temp);
                    dirs_rm.push(temp);
                } 
            } 
            d_entry = readdir(dir);
        } 
    }

    while(!(dirs_rm.empty())) {
        delete_dir(dirs_rm.top());
        dirs_rm.pop();
    }
    return err_msg;
}

string move_directory(string dir_og, string dest) {
    string err_msg="", dir_name, temp;
    struct dirent *d_entry;
    stack<string> dirs_rm;
    queue<string> dirs;
    struct stat d_info;
    string permissions;
    int index;
    DIR *dir;

    dirs.push(dir_og);
    while(!(dirs.empty())) {
        dir_name = dirs.front();
        dirs_rm.push(dir_name);
        dirs.pop();

        index = dir_name.find_last_of('/');
        if(index==string::npos) {
            err_msg="Full path not specified";
            break;
        }

        if(dest[dest.length()-1]!='/') {
            if(index!=0 && dir_name.length()>1) { 
                //not root directory
                dest = dest+"/"+dir_name.substr(index+1);
            } else {
                dest = dest+"/";
            }
        } else if(index!=0 && dir_name.length()>1) { 
            //not root directory and already has / at end
            dest+=dir_name.substr(index+1);       
        }

        //get permissions
        stat(dir_name.c_str(),&d_info);
        permissions = get_permissions(d_info);

        //create dir
        if(create_dir(dest,convert_permissions(permissions))!=0) {
            err_msg="Could not create new directory";
            break;
        }

        dir = opendir(dir_name.c_str());
        if(dir==NULL) {
            err_msg="Could not open current directory";
            break;
        }
        d_entry = readdir(dir);
        while(d_entry!=NULL) {
            temp = d_entry->d_name;
            if(temp!="." && temp!="..") {
                temp = dir_name+'/'+d_entry->d_name;
                stat(temp.c_str(), &d_info);

                //check file type
                if(S_ISREG(d_info.st_mode)) {
                    copy_file(temp,dest);
                    delete_file(temp);
                } else if(S_ISDIR(d_info.st_mode)) {
                    dirs.push(temp);
                    dirs_rm.push(temp);
                } 
            } 
            d_entry = readdir(dir);
        } 
    }

    while(!(dirs_rm.empty())) {
        delete_dir(dirs_rm.top());
        dirs_rm.pop();
    }
    return err_msg;
}

string copy_directory(string dir_og, string dest) {
    string err_msg="", dir_name, temp;
    struct dirent *d_entry;
    struct stat d_info;
    queue<string> dirs;
    string permissions;
    int index;
    DIR *dir;

    dirs.push(dir_og);
    while(!(dirs.empty())) {
        dir_name=dirs.front();
        dirs.pop();

        index = dir_name.find_last_of('/');
        if(index==string::npos) {
            err_msg="Full path not specified";
            break;
        }

        if(dest[dest.length()-1]!='/') {
            if(index!=0 && dir_name.length()>1) { 
                //not root directory
                dest = dest+"/"+dir_name.substr(index+1);
            } else {
                dest = dest+"/";
            }
        } else if(index!=0 && dir_name.length()>1) { 
            //not root directory and already has / at end
            dest+=dir_name.substr(index+1);       
        }

        //get permissions
        stat(dir_name.c_str(),&d_info);
        permissions = get_permissions(d_info);

        //create dir
        if(create_dir(dest,convert_permissions(permissions))!=0) {
            err_msg="Could not create new directory";
            break;
        }

        dir = opendir(dir_name.c_str());
        if(dir==NULL) {
            err_msg="Could not open current directory";
            break;
        }
        d_entry = readdir(dir);
        while(d_entry!=NULL) {
            temp = d_entry->d_name;
            if(temp!="." && temp!="..") {
                temp = dir_name+'/'+d_entry->d_name;
                stat(temp.c_str(), &d_info);

                //check file type
                if(S_ISREG(d_info.st_mode)) {
                    copy_file(temp,dest);
                } else if(S_ISDIR(d_info.st_mode)) {
                    dirs.push(temp);
                } 
            } 
            d_entry = readdir(dir);
        }    
    }
    return err_msg;
}

int create_file(string &file, mode_t m) {
    return creat(file.c_str(),m);
}

int delete_dir(string &directory) {
    return rmdir(directory.c_str());
}

int create_dir(string &directory, mode_t m) {
    return mkdir(directory.c_str(), m);
}

int rename_file(string &src, string &dest) {
    return rename(src.c_str(),dest.c_str());
}

int delete_file(string &file) {
    return unlink(file.c_str());
}

string copy_file(string &src, string &dest) {
    string err_msg="", temp1, temp2, permissions;
    int source_file, dest_file, index;
    struct stat d_info;
    char c;

    //get source filename from full path
    index = src.find_last_of('/');

    if(index==string::npos) {
        err_msg="Full path not provided";
    } else {
        temp1 = src.substr(index+1);
        temp2 = get_full_file_path(dest, temp1);

        source_file = open(src.c_str(),O_RDONLY);
        if(source_file==-1) {
            err_msg="Source file error";
        } else {
            //get permissions of source file
            stat(src.c_str(),&d_info);
            permissions = get_permissions(d_info);
            dest_file = create_file(temp2, convert_permissions(permissions));

            if(dest_file==-1) {
                err_msg = "Destination file error";
            } else {
                while(read(source_file,&c,1)) {
                    write(dest_file,&c,1);
                } 
                close(source_file);
                close(dest_file);
            }
        }
    }

    return err_msg;
}

string get_permissions(struct stat &d_info) {
    string permissions="";
    if(d_info.st_mode & S_IRUSR) {
        permissions+="r";
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IWUSR) {
        permissions+="w";
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IXUSR) {
        permissions+="x";
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IRGRP) {
        permissions+="r";        
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IWGRP) {
        permissions+="w";
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IXGRP) {
        permissions+="x";
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IROTH) {
        permissions+="r";
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IWOTH) {
        permissions+="w";
    } else {
        permissions+="-";
    }
    if(d_info.st_mode & S_IXOTH) {
        permissions+="x";
    } else {
        permissions+="-";
    }
    return permissions;
}

mode_t convert_permissions(string &permissions) {
    mode_t m=0;
    if(permissions[0]=='r') {
        m = m | 0400;
    }
    if(permissions[1]=='w') {
        m = m | 0200;
    }
    if(permissions[2]=='x') {
        m = m | 0100;
    }
    if(permissions[3]=='r') {
        m = m | 0040;
    }
    if(permissions[4]=='w') {
        m = m | 0020;
    }
    if(permissions[5]=='x') {
        m = m | 0010;
    }
    if(permissions[6]=='r') {
        m = m | 0004;
    }
    if(permissions[7]=='w') {
        m = m | 0002;
    }
    if(permissions[8]=='x') {
        m = m | 0001;
    }
    return m;
}

string get_parent(string path) {    
    //assuming that .. is present at end and is checked before calling this function
    string res=path;
    int index;

    index = path.find_last_of('/');
    if(index!=string::npos) {
        res=path.substr(0, index);
        index = res.find_last_of('/');
        if(index!=string::npos) {
            res=res.substr(0, index);
        }
    }
    return res;
}

bool comparator(vector<string> a, vector<string> b) {
    return (*(a.rbegin()) < *(b.rbegin()));
}

int list_out(string path) {   
    //returns 1 if error
    string permissions, temp_path, temp;
    vector<string> v_line;
    struct dirent *d_entry;
    struct stat d_info;
    struct passwd *pw;
    struct group *gr;
    char mtime[30];
    size_t index=0;
    struct tm lt;
    int col=0;
    time_t t;
    DIR *dir;

    sizelen=0;
    ownerlen=0;
    grouplen=0;

    dir = opendir(path.c_str());
    if(dir==NULL) {
        cout<<"Could not open current directory"<<endl;
        return 1;
    }
    d_entry = readdir(dir);
    d_output.clear();
    while(d_entry!=NULL) {
        v_line.clear();

        temp_path = path+'/'+d_entry->d_name;
        //get metadata for each entry
        stat(temp_path.c_str(), &d_info);

        //check file type
        if(S_ISREG(d_info.st_mode)) {
            permissions="-";
        } else if(S_ISDIR(d_info.st_mode)) {
            permissions="d";
        }

        //check permissions
        permissions+=get_permissions(d_info);

        //format time
        t = d_info.st_mtime;
        localtime_r(&t, &lt);
        strftime(mtime, sizeof(mtime), "%b %d %H:%M", &lt);

        pw = getpwuid(d_info.st_uid);
        gr = getgrgid(d_info.st_gid);

        v_line.push_back(permissions);
        v_line.push_back(pw->pw_name);
        v_line.push_back(gr->gr_name);
        v_line.push_back(to_string(d_info.st_size));
        v_line.push_back(mtime);
        v_line.push_back(temp_path);

        //to format display with correct spaces
        if(to_string(d_info.st_size).length()>sizelen) {
            sizelen = to_string(d_info.st_size).length();
        }
        temp = pw->pw_name;
        if(temp.length()>ownerlen) {
            ownerlen = temp.length();
        }
        temp = gr->gr_name;
        if(temp.length()>grouplen) {
            grouplen = temp.length();
        }

        d_output.push_back(v_line);
        d_entry = readdir(dir);
    }
    sort(d_output.begin(), d_output.end(), comparator);
    display();

    //get column position for cursor
    col_position=0;
    for(int i=0; i<(d_output[0].size()-1); ++i) {
        if(i==size_col_no) {
            col_position+=sizelen+1;
        } else if(i==owner_col_no) {
            col_position+=ownerlen+1;
        } else if(i==group_col_no) {
            col_position+=grouplen+1;
        } else {
            col_position+=d_output[0][i].size()+1;
        }
    }
    //counted space for each column of output except last so go forward 1 position
    ++col_position;
    return 0;
}

int nmode_keyinput() {  
    //return 1 for error
    struct termios original, newval;
    string temp, stack_string;
    size_t back_index;
    int err=0; 
    char key;

    //set terminal settings
	tcgetattr(0, &original);
	newval = original;
	newval.c_lflag &= ~ (ICANON | ECHO);
    if(tcsetattr(fileno(stdin), TCSAFLUSH, &newval)!=0) {
        cout<<"Could not set attributes for termios"<<endl;
        return 1;
    }

    set_position_value(row_cursor, col_cursor, d_output_index);
    position_cursor(row_cursor, col_cursor);
    
    while(1) {
        key = cin.get();
        if(key==ESC) {
            key = cin.get();
            key = cin.get();
            switch(key) {
                case K_UP:  
                            if(row_cursor>1) {
                                --row_cursor;
                                --d_output_index;
                                printf("%c[1A",ESC);
                            }
                            break;
                case K_DOWN: 
                            if(row_cursor<get_line_count() && row_cursor<size.ws_row-LEAVE_LINES) {
                                ++row_cursor;   
                                ++d_output_index;
                                printf("%c[1B",ESC);
                            }
                            break;
                case K_LEFT: 
                            if(back_stack.size()>1) {
                                start_index=end_index=-1;
                                stack_string = back_stack.top();
                                back_stack.pop();
                                err = list_out(back_stack.top());
                                if(err == 1) {
                                    break;
                                }
                                set_position_value(row_cursor, col_cursor, d_output_index);
                                position_cursor(row_cursor, col_cursor);
                                forward_stack.push(stack_string);
                            }
                            break;
                case K_RIGHT: 
                            if(!(forward_stack.empty())) {
                                start_index=end_index=-1;
                                stack_string = forward_stack.top();
                                forward_stack.pop();
                                err = list_out(stack_string);
                                if(err == 1) {
                                    break;
                                }
                                set_position_value(row_cursor, col_cursor, d_output_index);
                                position_cursor(row_cursor, col_cursor);
                                back_stack.push(stack_string);
                            }
                            break;
            }
        } else if(key==24) {    
            //To detect Cntrl+X
            break;
        } else if(key==10) {    
            //To detect enter
            //Check if selected file is a directory
            if(d_output[d_output_index][0][0]=='d') {
                start_index=end_index=-1;
                stack_string = d_output[d_output_index][d_output[0].size()-1];

                if(stack_string[stack_string.length()-1]=='.' && stack_string[stack_string.length()-2]=='.') {
                    stack_string = get_parent(stack_string);
                } 

                back_stack.push(stack_string);
                clear_stack(forward_stack);
                err = list_out(stack_string);
                if(err == 1) {
                    break;
                }
                set_position_value(row_cursor, col_cursor, d_output_index);
                position_cursor(row_cursor, col_cursor);
            } else {
                //open file using vi editor
                pid_t pid = fork();
                if(pid==0) {
                    char t1[d_output[d_output_index][d_output[0].size()-1].length()];
                    stpcpy(t1, d_output[d_output_index][d_output[0].size()-1].c_str());
                    char *arg_list[3] = {(char*)"vi",t1,NULL};
                    execvp("vi",arg_list);
                } 
                wait(NULL);

                //reset screen
                start_index=end_index=-1;
                err = list_out(back_stack.top());
                if(err == 1) {
                    break;
                }
                set_position_value(row_cursor, col_cursor, d_output_index);
                position_cursor(row_cursor, col_cursor);
            }
        } else if(key==107) {   
            //k = scroll up 
            --start_index;
            --end_index;
            display();
            set_position_value(row_cursor, col_cursor, d_output_index);
            position_cursor(row_cursor, col_cursor);
        } else if(key==108) {   
            //l = scroll down
            ++start_index;
            ++end_index;
            display();
            set_position_value(row_cursor, col_cursor, d_output_index);
            position_cursor(row_cursor, col_cursor);
        } else if(key==104) {   
            //Go to home on pressing h
            start_index=end_index=-1;
            back_stack.push(cwd);
            clear_stack(forward_stack);
            err = list_out(cwd);
            if( err == 1) {
                break;
            }
            set_position_value(row_cursor, col_cursor, d_output_index);
            position_cursor(row_cursor, col_cursor);
        } else if(key==127) {   
            //backspace => go to parent directory
            if(!back_stack.empty()) {
                stack_string = back_stack.top();
                clear_stack(forward_stack);
                back_index = stack_string.find_last_of('/');
                if(back_index==string::npos) {
                    cout<<"Error finding parent of current path"<<endl;
                    err=1;
                    break;
                }
                start_index=end_index=-1;
                if(back_index) {
                    stack_string = stack_string.substr(0, back_index);
                    back_stack.push(stack_string);
                } else {    
                    //If its the root directory then include /
                    stack_string=stack_string.substr(0, back_index+1);
                }
                err = list_out(stack_string);
                if(err == 1) {
                    break;
                }
                set_position_value(row_cursor, col_cursor, d_output_index);
                position_cursor(row_cursor, col_cursor);
            }
        } else if(key==58) {    
            //: => enter command mode
            //call cmode function
            err = cmode_keyinput();
            if(err==1) {
                break;
            }
            //call ls
            start_index=end_index=-1;
            err = list_out(back_stack.top());
            if(err == 1) {
                break;
            }
            set_position_value(row_cursor, col_cursor, d_output_index);
            position_cursor(row_cursor, col_cursor);
        }
    }

    //reset terminal settings
	tcsetattr(fileno(stdin), TCSANOW, &original);
    return err;
}

int cmode_keyinput() {
    vector<string> command;
    string str="", word="", err_msg="";
    int err=0, xpos=0;
    
    char key;

    initialise_display_command_mode();
    command.clear();

    while(1) {
        key=cin.get();
        if(key!=10 && key!=127) {
            cout<<key;
            str+=key;
        }        
        if(key==ESC) {   
            //Go to normal mode
            break;
        } else if(key==10) {    
            //On enter

            //position cursor at last line
            position_cursor(size.ws_row,1);
            printf("%c[K",ESC);

            //Save cursor position
            xpos = str.length()+3;

            //split str
            stringstream ss(str);
            while(ss>>word) {
                command.push_back(word);
            }

            //check str for command
            err_msg = execute_command(command);

            if(err_msg.length()) {
                //position cursor at last line
                position_cursor(size.ws_row,1);

                //print error message
                cout<<err_msg;

                //restore cursor position
                position_cursor(size.ws_row-1,xpos);
            } else {
                position_cursor(size.ws_row-1,3);
                printf("%c[K",ESC);
                str.clear();
            }

            command.clear();
            err_msg.clear();

        } else if(key==127) {   
            //backspace
            if(str.length()) {
                printf("%c[1D",ESC);
                printf("%c[K",ESC);
                str.resize(str.length()-1);
            }
        }
    }

    return err;
}