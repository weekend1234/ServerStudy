# ServerStudy
C++ Windows/Linux Server Study

* 리눅스 빌드 
리눅스의 경우 빌드 후 WinSCP 나 Putty 등을 이용해서 
~/projects/솔루션폴더/ 안에 resources 폴더를 만들고 
ServerConfig.ini 파일을 리눅스 쪽에 넣어줘야 돌아갑니다.

* 리눅스 포트 열어주기
firewall-cmd --zone=public --permanent --add-port=32452/tcp (32452에 ServerConfig.ini에서 입력해준 포트 입력)
firewall-cmd --reload
firewall-cmd --zone=public --list-all

* CentOS7 C++17
(CentOS는 yum install 로 설치할 수 있는 버전에 한계가 있음)
yum install centos-release-scl
yum install devtoolset-8-gcc
scl enable devtoolset-8 bash
which gcc
gcc --version

이후 아래 내용을 각 유저별 .bashrc 에 추가하면 C++17 사용 가능
source /opt/rh/devtoolset-8/enable

