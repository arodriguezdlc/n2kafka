
node ('docker') {
    stage 'Docker preparation'
    git branch: 'master', credentialsId: 'jenkins_id', url: 'git@gitlab.redborder.lan:core-developers/rb-runner-c.git'
    def cbuildsbase = docker.build "rb-cbuilds-base:latest", "base"
    def n2kafkabuild = docker.build "rb-cbuilds-n2kafka:latest", "n2kafka"
    stage 'Build'
    n2kafkabuild.inside {
        git branch: 'continuous-integration', credentialsId: 'jenkins_id', url: 'git@gitlab.redborder.lan:dfernandez.ext/n2kafka.git'
        sh './configure --disable-optimization'
        sh 'make'
        stage 'Unit Tests'
        sh 'make tests'
    }
}
