import matplotlib.pyplot as plt
import os


cubic_data_file = "lab2-part1-ref-TcpCubic-1flows-sock0-cwnd.dat"
newreno_data_file = "lab2-part1-ref-TcpNewReno-1flows-sock0-cwnd.dat"


def parse_data_from_file(filename):
    times = []
    cwnds = []

    # Verifica se o arquivo existe
    if not os.path.exists(filename):
        print(f"ERRO: Arquivo não encontrado: {filename}")
        print(
            "Por favor, execute a simulação 1a primeiro ou verifique o nome do arquivo."
        )
        return None, None

    try:
        with open(filename, "r") as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) == 2:
                    try:
                        times.append(float(parts[0]))
                        cwnds.append(int(parts[1]))
                    except ValueError:
                        print(
                            f"Ignorando linha mal formada em {filename}: {line.strip()}"
                        )
    except Exception as e:
        print(f"Erro ao ler o arquivo {filename}: {e}")
        return None, None

    return times, cwnds


cubic_times, cubic_cwnds = parse_data_from_file(cubic_data_file)
newreno_times, newreno_cwnds = parse_data_from_file(newreno_data_file)

if cubic_times is None or newreno_times is None:
    print("Plotagem cancelada devido a erros na leitura dos arquivos.")
else:
    plt.figure(figsize=(14, 7))

    plt.plot(
        cubic_times,
        cubic_cwnds,
        label="TCP CUBIC",
        color="green",
        linestyle="-",
        marker="o",
        markersize=4,
        alpha=0.7,
    )

    plt.plot(
        newreno_times,
        newreno_cwnds,
        label="TCP NewReno",
        color="yellow",
        linestyle="-",
        marker="v",
        markersize=4,
        alpha=0.7,
    )

    plt.title("Comparação da Congestão (Cwnd) x Tempo", fontsize=16)
    plt.xlabel("Tempo em (s)", fontsize=12)
    plt.ylabel("Congestão em (bytes)", fontsize=12)

    plt.legend(fontsize=12)
    plt.grid(True, linestyle=":", alpha=0.6)

    plt.tight_layout()

    output_filename = "graphComparacao.png"
    plt.savefig(output_filename)

    print(f"Gráfico salvo como: {output_filename}")
