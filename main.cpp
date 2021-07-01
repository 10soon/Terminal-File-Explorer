#include "utility_v2.h"
using namespace std;

int main() {
    int err=0;

    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        cout<<"Error on executing getcwd()"<<endl;
        exit(0);
    }

    back_stack.push(cwd);
    //ls for current directory
    list_out(cwd);

    err = nmode_keyinput();
    if(!err) {
        clearscreen();
    }
    return 0;
}