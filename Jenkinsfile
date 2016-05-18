
node ('docker') {
    stage 'Clone rb-runner-c repo' 
    git branch: 'master', credentialsId: 'jenkins_id', url: 'git@gitlab.redborder.lan:core-developers/rb-runner-c.git'
    stage 'Build rb-cbuilds-base image'
    def cbuildsbase = docker.build "rb-cbuilds-base:latest", "base"
    stage 'Build rb-cbuilds-n2kafka image'
    def n2kafkabuild = docker.build "rb-cbuilds-n2kafka:latest", "n2kafka"
    stage 'Compile'
    n2kafkabuild.inside {
        git branch: 'continuous-integration', credentialsId: 'jenkins_id', url: 'git@gitlab.redborder.lan:dfernandez.ext/n2kafka.git'
        sh './configure --disable-optimization'
        sh 'make'
        stage 'running tests'
        sh 'CFLAGS=-w make -s coverage'
	//sh 'make tests'
	stage 'Copy binaries'
	stash includes: 'n2kafka', name: 'n2kafka'
    }
}

node ('prep-manager') {
   stage 'Deploy to preproduction'
   unstash 'n2kafka'
   sh 'cp n2kafka /opt/rb/bin/n2kafka'
   sh '/opt/rb/bin/rb_manager_scp.sh all /opt/rb/bin/n2kafka'
   sh '/opt/rb/bin/rb_service all restart http2k'
}
