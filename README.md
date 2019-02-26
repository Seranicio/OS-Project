# OS-Project
Operating Systems Project in 2016/2017

# Running

Use Google translate:

"A aplicação corre fazendo, em terminal, a instrução make, e posteriormente correr com
a instrução ./main.
De seguida através dum browser acedemos às várias páginas como por exemplo:
• Localhost:<port>/índex.html
• Localhost:<port>/dijkstra.html
• Localhost:<port>/<ficheiro>
Nesta situação <port> é substituído pelo port utilizado – no nosso caso 5000 – podendo,
no entanto, ser alterado no ficheiro de configurações e <ficheiro> poderá ser substituído por
um ficheiro de extensão .gz existente na pasta da aplicação. É de notar que, apesar da
existência do ficheiro, poderá não ser enviado o conteúdo do mesmo ao cliente, devido às
autorizações disponibilizadas pelas configurações do servidor.
Assim o pedido ao browser responderá ao cliente da seguinte forma:
• Se for uma página estática .html, existente mostrará o seu conteúdo ao cliente;
• Se for uma página comprimida, .gz, e o ficheiro estiver autorizado pelas
configurações de servidor, também irá mostrar o conteúdo ao cliente;
• Se for uma página comprimida, .gz, e o ficheiro não estiver autorizado ou não
estiver na pasta do servidor, será enviado ao utilizador o devido aviso de
impossibilidade de acesso.
Para finalizar é necessário o comando CTRL+C que fará, através de um SIGINT, uma cleanup das
várias componentes como por exemplo, fechar o manager pipe, fechar os sockets, limpar e sair
das threads, limpar a shared memory e limpar os buffers.
É possível ainda, num terminal à parte, correr o executável manager, para fazer acções
como mudar o tipo de escalonamento, alterar a pool de threads, ou adicionar/remover os
ficheiros autorizados.
Quanto à obtenção de estatísticas, tal como nos é pedido, temos um ficheiro, de nome
server.log, que nos dá informação sobre os pedidos feitos ao servidor, o ficheiro html, a hora de
recepção do pedido e a hora de terminação do pedido após o envio do resultado ao cliente.
Através de um sinal SIGUSR1 temos informações sobre o número total de pedidos de páginas
estáticas e de ficheiros comprimidos e o tempo médio de resposta a cada tipo de conteúdo.
Para finalizar temos um SIGUSR2 que faz um reset completo às estatísticas do SIGUSR1.
Para a execução dos SIGUSR é necessário, num terminal novo, executar a linha kill –
SIGUSR<1/2> <pid stats>, em que <1/2> é o SIGUSR a executar e <pid stats> é o process ID da
estatística."


