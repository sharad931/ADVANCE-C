#include<iostream>
using namespace std;
int solve(int num){
    int sum=0;
    while(num>0){
        int ld=num%10;
        sum=sum+ld;
        num=num/10;

    }

    return sum;
}
int main(){

    int n=256;
    int ans=solve(n);
    cout<<ans;

    return 0;
}